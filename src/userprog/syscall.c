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

#define MAX_SYSCALLS 21

typedef int pid_t;

static void syscall_handler (struct intr_frame *);
static void try_acquiring_filesys(void);
static void try_releasing_filesys(void);
bool is_vaddr(const void *uaddr);
static void *syscall_handlers[MAX_SYSCALLS];

static void halt(void); 
static void exit (int status);
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

struct file_wrapper {
    struct file *file;
    struct list_elem file_elem;
    int fd;
};

static int get_next_fd(void);
struct file_wrapper *get_file_by_fd (int fd);

//should we make argument to this function void* or const char *?
static void check_fp_valid(void *file);
struct lock lock_filesys;

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
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
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

  int arg1 = is_vaddr((void *) (esp + 1)) ? *(esp + 1) : (int) NULL;
  int arg2 = is_vaddr((void *) (esp + 2)) ? *(esp + 2) : (int) NULL;
  int arg3 = is_vaddr((void *) (esp + 3)) ? *(esp + 3) : (int) NULL;

  f->eax = function(arg1,arg2,arg3);

}

static void halt(void) {
  shutdown_power_off();
}

static void exit (int status) {
  struct thread *cur = thread_current();
  lock_acquire(&cur->exit_status->lock);
  cur->exit_status->exit_code = status;
  cur->exit_status->exited = true;
  lock_release(&cur->exit_status->lock);
  thread_exit();
}

static pid_t exec (const char *file) {
  // check_fp_valid((char *) file);

  pid_t pid = -1;
  if (!is_vaddr(file)) {
    return pid;
  }
  char *arg;
  char *save_ptr, *mod_fn;

  mod_fn = palloc_get_page (0);
  if (mod_fn == NULL)
    return pid;
  strlcpy (mod_fn, file, PGSIZE);
  arg = strtok_r(mod_fn," ", &save_ptr);

  struct file *f = filesys_open(arg);
  if (f == NULL) {
    return -1;
  } else {
    file_close(f);
  }
  palloc_free_page(mod_fn);
  try_acquiring_filesys();
  pid = process_execute(file);
  try_releasing_filesys();
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
  try_acquiring_filesys();
  bool success = filesys_create(file, initial_size);
  try_releasing_filesys();
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

  try_acquiring_filesys();
  if (!is_vaddr((struct inode *) file)) {
    return -1;
  }
  opened_file = filesys_open(file);
  try_releasing_filesys();

  if (opened_file == NULL) {
    return -1;
  }
  // Do not forget to free
  wrapped_file = malloc (sizeof(struct file_wrapper));
  if (wrapped_file == NULL) {
    return -1;
  }
  try_acquiring_filesys();
  wrapped_file->file = opened_file;
  wrapped_file->fd = get_next_fd();
  list_push_back(&thread_current()->opened_files, &wrapped_file->file_elem);
  try_releasing_filesys();

  return wrapped_file->fd;  
}


static bool remove (const char *file) {
  check_fp_valid((char *) file);
  // check if filename is empty, in which case can't remove
  if (!strcmp(file, "")) {
    return false;
  }
  try_acquiring_filesys();
  bool deleted_file = filesys_remove(file);
  try_releasing_filesys();
  return deleted_file;
}


static int filesize (int fd UNUSED) {
  return 0;
}

static int read (int fd UNUSED, void *buffer UNUSED, unsigned length UNUSED) {
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
    try_acquiring_filesys();
    length_read = file_read(f->file, buffer, length);
    try_releasing_filesys();
  }
  // printf("returning with length %i\n", length_read);
  return length_read;
}

static void seek (int fd UNUSED, unsigned position UNUSED) {
  struct file_wrapper *file;
  file = get_file_by_fd(fd);
  if (file == NULL) {
    exit(-1);
  }
  try_acquiring_filesys();
  file_seek(file->file, position);
  try_releasing_filesys();
}

static unsigned tell (int fd UNUSED) {
  struct file_wrapper *file;
  unsigned pos;
  file = get_file_by_fd(fd);
  if (file == NULL) {
    exit(-1);
  }
  try_acquiring_filesys();
  pos = file_tell(file->file);
  try_releasing_filesys();
  return pos;
}

static void close (int fd UNUSED) {
  struct file_wrapper *file;
  try_acquiring_filesys();
  file = get_file_by_fd(fd);
  if (file == NULL) {
    exit(-1);
  }
  list_remove(&file->file_elem);
  file_close(file->file);
  free(file);
  try_releasing_filesys();
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
    try_acquiring_filesys();
    length_write = file_write(f->file, buffer, length);
    try_releasing_filesys();
  }
  // TODO: Writing to an actual file
  return length_write;
}

//should we make argument to this function void* or const char *?
static void check_fp_valid(void* file) {
  //check if pointer to file passed in is valid
  if ((file) || (!is_vaddr(file))) {
    exit(-1);
  }
  if (!pagedir_get_page(thread_current()->pagedir, file)) {
    free(file);
    exit(-1);
  }
}

static void try_acquiring_filesys() {
  if (lock_filesys.holder != thread_current()) {
    lock_acquire(&lock_filesys);
  }
}

static void try_releasing_filesys() {
  if (lock_filesys.holder != thread_current()) {
    lock_release(&lock_filesys);
  }
}

static int get_next_fd() {
  static int next_fd = 2;
  int fd;
  try_acquiring_filesys();
  fd = next_fd++;
  try_releasing_filesys();
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
  if (uaddr == NULL)
    return false;
  if (!is_user_vaddr(uaddr))
    return false;
  return pagedir_get_page(thread_current()->pagedir, uaddr) != NULL;
}