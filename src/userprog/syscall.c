#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "lib/kernel/stdio.h"
#include "userprog/syscall.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "lib/kernel/console.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "devices/input.h"
#include "vm/mmap.h"
#include "vm/page.h"
#include "vm/frame.h"


#define MAX_SYSCALLS 21

typedef int pid_t;

static void syscall_handler (struct intr_frame *);
bool is_vaddr(const void *uaddr);
static void *syscall_handlers[MAX_SYSCALLS];

static void halt(void); 
static pid_t exec (const char *file);
static int wait (pid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned length);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);
static mapid_t mmap(int fd, void* addr);


static int get_next_fd(void);
struct file_wrapper *get_file_by_fd (int fd);

static struct lock lock_filesys;
static mapid_t get_next_mapid(void);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&lock_filesys);
  syscall_handlers[SYS_HALT] = &halt;
  syscall_handlers[SYS_EXIT] = &exit;
  syscall_handlers[SYS_EXEC] = &exec;
  syscall_handlers[SYS_WAIT] = &wait;
  syscall_handlers[SYS_CREATE] = &create;
  syscall_handlers[SYS_REMOVE] = &remove;
  syscall_handlers[SYS_OPEN] =  &open;
  syscall_handlers[SYS_FILESIZE] = &filesize;
  syscall_handlers[SYS_READ] = &read;
  syscall_handlers[SYS_WRITE] = write;
  syscall_handlers[SYS_SEEK] =  &seek;
  syscall_handlers[SYS_TELL] = &tell;
  syscall_handlers[SYS_CLOSE] = &close;
  syscall_handlers[SYS_MMAP] = &mmap;
  syscall_handlers[SYS_MUNMAP] = &munmap;
}

static void
syscall_handler (struct intr_frame *f ) 
{
  uint32_t *esp = (uint32_t *)f->esp;
  // Check if address of system call is valid before dereferencing
  if (!is_vaddr(esp))
    exit (-1);
  int system_call_number = *esp;

  // Check if system call is valid
  if (system_call_number < SYS_HALT || system_call_number > MAX_SYSCALLS)
    exit (-1);

  // Generic Function wrapper
  int (*function) (int, int, int) = syscall_handlers[system_call_number];
  // Max 3 arguments, if an argument is not valid then it just passes null
  // the actual function will deal with the arguments it needs

  // If first argument is invalid then the rest are too so exit with code -1
  if (!is_vaddr((void *) (esp + 1))) {
    exit(-1);
  }
  // Validates each pointer, if pointer is not valid passes NULL; 
  int arg1 = is_vaddr((void *) (esp + 1)) ? *(esp + 1) : (int) NULL;
  int arg2 = is_vaddr((void *) (esp + 2)) ? *(esp + 2) : (int) NULL;
  int arg3 = is_vaddr((void *) (esp + 3)) ? *(esp + 3) : (int) NULL;

  f->eax = function(arg1,arg2,arg3);
}

static void halt(void) {
  shutdown_power_off();
}

void exit(int status) {
  struct thread *cur = thread_current();
  // Release lock, if held by thread about to exit
  if (lock_held_by_current_thread(&lock_filesys)) {
    lock_release(&lock_filesys);
  }
  // Free mmap files in process exit or here
  cur->exit_status->exit_code = status;
  
  thread_exit();
}

static pid_t exec (const char *file) {
  pid_t pid = -1;
  if (!is_vaddr(file)) {
    return pid;
  }

  lock_acquire(&lock_filesys);
  pid = process_execute(file);
  lock_release(&lock_filesys);
  return pid;
}

static int wait (pid_t pid) {
  return process_wait(pid);
}

static bool create (const char *file, unsigned initial_size) {
  if (file == NULL || !is_vaddr(file)) {
    exit(-1);
  }
  // Currently passess all tests for create but needs to be checked
  //all checks complete. create file
  lock_acquire(&lock_filesys);
  bool success = filesys_create(file, initial_size);
  lock_release(&lock_filesys);
  return success;
}

/*Opens the file called file. 
  Returns a nonnegative integer handle called a “file descriptor” (fd), 
  or -1 if the file could not be opened.*/

static int open (const char *file) {
  if (!is_vaddr(file)) {
    exit(-1);
  }
  if (strcmp(file, "") == 0) {
    return -1;
  }
  struct file *opened_file;
  struct file_wrapper *wrapped_file;

  lock_acquire(&lock_filesys);
  if (!is_vaddr((struct inode *) file)) {
    return -1;
  }
  opened_file = filesys_open(file);
  lock_release(&lock_filesys);

  if (opened_file == NULL) {
    return -1;
  }
  // Is freed in exit()
  wrapped_file = malloc (sizeof(struct file_wrapper));
  if (wrapped_file == NULL) {
    return -1;
  }
  lock_acquire(&lock_filesys);
  wrapped_file->file = opened_file;
  wrapped_file->fd = get_next_fd();
  list_push_back(&thread_current()->opened_files, &wrapped_file->file_elem);
  lock_release(&lock_filesys);

  return wrapped_file->fd;  
}


static bool remove (const char *file) {
  // check_fp_valid((char *) file);
  
  // check if filename is empty, in which case can't remove
  lock_acquire(&lock_filesys);
  struct file *f = filesys_open(file);
  if (f == NULL) {
    exit(-1);
  } else {
    file_close(f);
  }
  bool deleted_file = filesys_remove(file);
  lock_release(&lock_filesys);
  return deleted_file;
}


static int filesize (int fd ) {
  struct file_wrapper *fw = get_file_by_fd(fd);
  if (fw == NULL) {
    return -1;
  }
  lock_acquire(&lock_filesys);
  int size = file_length(fw->file);
  lock_release(&lock_filesys);
  return size;
}

static int read (int fd, void *buffer, unsigned length) {
  int length_read = 0;
  // Check to see if the addresses from where we are reading up until the end 
  // are valid, ottherwise exit with status code -1
  if (!is_vaddr(buffer) || !is_vaddr(buffer + length)) {
    exit(-1);
  } 
  if (fd == STDOUT_FILENO) {
    return -1;
  }
  if (fd == STDIN_FILENO) {
    for (int i= 0; i < (int) length; i++) {
      *(uint8_t*) buffer = input_getc();
      (buffer)++;
    return length;
    }
  } else {
    struct file_wrapper *f = get_file_by_fd(fd);
    if (f == NULL) {
      return -1;
    }
    lock_acquire(&lock_filesys);
    length_read = file_read(f->file, buffer, length);
    lock_release(&lock_filesys);
  }
  // printf("returning with length %i\n", length_read);
  return length_read;
}

static void seek (int fd , unsigned position ) {
  struct file_wrapper *file;
  file = get_file_by_fd(fd);
  if (file == NULL) {
    exit(-1);
  }
  lock_acquire(&lock_filesys);
  file_seek(file->file, position);
  lock_release(&lock_filesys);
}

static unsigned tell (int fd ) {
  struct file_wrapper *file;
  unsigned pos;
  file = get_file_by_fd(fd);
  if (file == NULL) {
    exit(-1);
  }
  lock_acquire(&lock_filesys);
  pos = file_tell(file->file);
  lock_release(&lock_filesys);
  return pos;
}

static void close (int fd ) {
  struct file_wrapper *file;
  file = get_file_by_fd(fd);
  if (file == NULL) {
    exit(-1);
  }
  lock_acquire(&lock_filesys);
  list_remove(&file->file_elem);
  file_close(file->file);
  free(file);
  lock_release(&lock_filesys);
}

static int write (int fd, const void *buffer, unsigned length) {
  // If no bytes have been written return default of 0
  int length_write = 0;
  if (!is_vaddr(buffer) || !is_vaddr(buffer + length)) {
    exit(-1);
  } 
  if (fd == STDIN_FILENO) {
    return 0;
  }
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, length);
    length_write = length;
  } else {
    struct file_wrapper *f = get_file_by_fd(fd);
    if (f == NULL) {
      return -1;
    }
    lock_acquire(&lock_filesys);
    length_write = file_write(f->file, buffer, length);
    lock_release(&lock_filesys);
  }
  return length_write;
}


/*Maps the file open as fd into the process’s virtual address space. 
  The entire file is mapped into consecutive virtual pages starting at addr. */
mapid_t mmap(int fd, void* addr) {
  struct file* file = file_reopen(get_file_by_fd(fd)->file);
  size_t file_size = file_length(file);

  /* If file is not opened/not valid and filesize <= 0 then return -1*/ 
  if (file == NULL || file_size <= 0) {
    return -1;
  }
  
  /*First check:  Checks if address is page aligned
    Second check: checks if address is NULL and by extension whether its virtual page 0
    Third check:  Checks if address is a user virtual address and not a kernel virtual address*/
  if (pg_ofs(addr) != 0 || addr == NULL || !is_user_vaddr(addr)) {
    return -1;
  }
  
  /* Checks whether File descriptor passed in is 0 or 1 
    which are reserved std input and output for */
  if (fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return -1;
  }

  void* temp = addr;
  void* start_addr = addr;
  void* end_addr = addr;

  while (file_size > 0) {

    size_t read_bytes;
    size_t zero_bytes;
    size_t ofs = 0;
    bool writable = true;

    if (file_size >= PGSIZE) {
      read_bytes = PGSIZE;
      zero_bytes = 0;    
    } else {
      read_bytes = file_size;
      zero_bytes = PGSIZE - file_size;
    }
    
    /* Check if there already exist a loaded kpage */ 
    uint8_t *kpage = pagedir_get_page (thread_current()->pagedir, temp);
    /* If there already exist a loaded kpage 
       then we have an overlap with an already loaded page 
       be it a zeroed page (stack) or file page */
    if (kpage != NULL)
      return -1;

    /* If there doesn't exist a loaded kpage it's possible that this page has been loaded lazily
       So we need to check if there exists an entry in our Supplemental Page Table */ 
    struct spt_entry *loaded = spt_find_addr(temp);
    if (loaded != NULL) {
      return -1;
    }

    /* Obtain a free frame*/
    kpage = obtain_free_frame(PAL_USER);
    if (kpage == NULL){
      return -1;
    }

    /* Create a file SPT entry */
    struct spt_entry *spt_page = create_file_page(file, temp, ofs, read_bytes, zero_bytes, writable);
    if (spt_page == NULL)
      return -1;
    /* Add it to the SPT */
    spt_add_page(&thread_current()->spt, spt_page);

    /* Advance */
    file_size -= read_bytes;
    ofs += PGSIZE;
    temp += PGSIZE;
    end_addr += read_bytes;
  }
  /* Now that we have loaded all the pages lazily into the SPT we can create and add 
     The Memory Mapped File to our list */
  mapid_t mapid = get_next_mapid();
  insert_mfile(mapid, file, start_addr, end_addr);

  return mapid;
}

void munmap (mapid_t mapid) {
  struct memory_file *mfile = get_mfile(mapid);
  /* If no memory mapping exists then exit -1 either due it not being mapped
     or already being unmapped*/
  if (mfile == NULL) {
    exit(-1);
  }
  size_t ofs = 0;
  for (void *addr = mfile->start_addr; addr < mfile->end_addr; addr+=PGSIZE, ofs+=PGSIZE) {
    struct spt_entry *page = spt_find_addr(addr);
    /* Check if spt entry exists */
    if (page != NULL) {
      /* If it does exist then check if the page has been modified since its been added */
      if(pagedir_is_dirty(thread_current()->pagedir, addr)) {
        /* If it has been modified then write the modified page back to the file*/
        file_seek(mfile->file, ofs);
        file_write(mfile->file, page->upage, page->read_bytes);
      }
      /* Check if the page has actually been loaded into memory*/
      void *kpage = pagedir_get_page(thread_current()->pagedir, addr);
      if (kpage != NULL) {
        /* If so then free the frame in memory that belongs to that page*/
        free_frame_from_table(kpage);
      }
      /* Clear the page - sets not_present to true - will pagefault and exit on access*/
      pagedir_clear_page(thread_current()->pagedir, page->upage);
      /* Remove page from Supplemental Page Table*/
      spt_delete_page(&thread_current()->spt, page->upage);
    }
  }
  /* Delete the Memory Mapped File from the list - frees the struct also*/
  delete_mfile(mfile);
}

/* Gets the next File Descriptor*/
static int get_next_fd() {
  static int next_fd = 2;
  int fd;
  fd = next_fd++;
  return fd;
}

/* Gets the next Mapped File ID */
static mapid_t get_next_mapid(void) {
  static mapid_t next_mapid = 0;
  return next_mapid++;
}

/* USERPROG Function, given the tid returns the thread with that tid*/
struct file_wrapper *
get_file_by_fd (int fd)
{
  struct list_elem *elem;
  elem = list_begin (&thread_current()->opened_files);
  while (elem != list_end (&thread_current()->opened_files)) {
    struct file_wrapper *f = list_entry (elem, struct file_wrapper, file_elem);
    if (f->fd == fd)
      return f;
    elem = list_next (elem);
  }
  return NULL;
}

/* Validate a user's pointer to a virtual address
  must not be
    - null pointer
    - pointer to unmapped virtual memory
    - pointer to kernel virtual address space (above PHYS_BASE).
  All of these types of invalid pointers must be rejected without harm to the
  kernel or other running processes, by terminating the offending process and
  freeing its resources. */
bool is_vaddr(const void *uaddr)
{
  struct thread *t = thread_current();
  if (uaddr == NULL)
    return false;
  if (!is_user_vaddr(uaddr))
    return false;
  /* Now that we have VM checking if a kpage exists is not enough as a page could've been loaded lazily*/
  if (pagedir_get_page(thread_current()->pagedir, uaddr) == NULL) {
    struct spt_entry temp;
    temp.upage = (void *) uaddr;
    /* Therefore if it's NULL we have to check if an entry exists in the SPT
       If there doesn't exist an entry in the SPT either then we are sure this is not
       a valid address
    */
    struct hash_elem *elem = hash_find(&t->spt, &temp.hash_elem);
    if (elem == NULL) 
      return false;
    
    return true;
  }
  return true;
}