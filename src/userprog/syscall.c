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
static void munmap(mapid_t mapping);

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
    // Writing to the console
    // returns number of bytes written
    /* TODO: Keep in mind edge case where number of bytes written
    is less than size because some were not able to be written. 
    */
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

//map the file open denoted by file descriptor fd into process's vaddr space
mapid_t mmap(int fd, void* addr) {
  size_t file_size = filesize(fd);
  struct file* file = file_reopen(get_file_by_fd(fd)->file);

  //check if file is invalid
  if (file == NULL || file_size <= 0) {
    return -1;
  }

  //check if address is invalid
  if (pg_ofs(addr) != 0 || addr == NULL || addr == 0x0) {
    return -1;
  }
  
  if (fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return -1;
  }

  //check if filelength is multiple of PGSIZE
  while (file_size > 0) {

    size_t read_bytes;
    size_t zero_bytes;
    void* temp = addr;
    size_t ofs = 0;

    struct page* page;

    if (file_size >= PGSIZE) {
      read_bytes = PGSIZE;
      zero_bytes = 0;    
    } else {
      read_bytes = file_size;
      zero_bytes = PGSIZE - file_size;
    }

    if (find_page(temp) != NULL) {
      return -1;
    }

    page = new_file_page (temp, file, ofs, read_bytes, zero_bytes, true);

    
    ofs += PGSIZE;
    file_size -= read_bytes;
    temp += PGSIZE;


  }

  mapid_t mapid = get_next_mapid();


  return mapid;

}


void munmap(mapid_t mapping) {
  return;
}


static int get_next_fd() {
  static int next_fd = 2;
  int fd;
  fd = next_fd++;
  return fd;
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


static mapid_t get_next_mapid(void) {
  static mapid_t next_mapid = 0;
  return next_mapid++;
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
  if (pagedir_get_page(thread_current()->pagedir, uaddr) == NULL) {
    struct spt_entry temp;
    temp.upage = uaddr;
    struct hash_elem *elem = hash_find(&t->spt, &temp.hash_elem);
    if (elem == NULL) 
      exit(-1);
    volatile char pgfault_byte = *(char*) uaddr;
    return pagedir_get_page(t->pagedir, uaddr) != NULL;
  }
  return true;
}