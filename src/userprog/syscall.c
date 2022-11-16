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

#define MAX_SYSCALLS 21
static void syscall_handler (struct intr_frame *);
void read_args(void* esp, int num_args, void **args);
void try_acquiring_filesys(void);
void try_releasing_filesys(void);
bool is_vaddr(const void *uaddr);
static void *syscall_handlers[MAX_SYSCALLS];

int halt_handler(struct intr_frame *f UNUSED);
int exit_handler(struct intr_frame *f UNUSED); 
int exec_handler(struct intr_frame *f UNUSED);
int wait_handler(struct intr_frame *f UNUSED);
int create_handler(struct intr_frame *f UNUSED);
int remove_handler(struct intr_frame *f UNUSED); 
int open_handler(struct intr_frame *f UNUSED); 
int filesize_handler(struct intr_frame *f UNUSED);
int read_handler(struct intr_frame *f UNUSED); 
int write_handler(struct intr_frame *f UNUSED); 
int seek_handler(struct intr_frame *f UNUSED); 
int tell_handler(struct intr_frame *f UNUSED); 
int close_handler(struct intr_frame *f UNUSED); 

//should we make argument to this function void* or const char *?
void check_fp_valid(void *file);

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
  if (system_call_number < SYS_HALT || system_call_number > SYS_INUMBER)
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

void halt(void) {
  shutdown_power_off();
}

void exit (int status) {
  struct thread *cur = thread_current();
  lock_acquire(&cur->exit_status->lock);
  cur->exit_status->exit_code = status;
  cur->exit_status->exited = true;
  lock_release(&cur->exit_status->lock);
  thread_exit();
}

pid_t exec (const char *file) {
  check_fp_valid((char *) file);

  char *arg;
  char *save_ptr;

  int filename_length = strlen(file);
  char filename_copy[filename_length];
  strlcpy(filename_copy, file, filename_length);
  arg = strtok_r(filename_copy, " ", &save_ptr);

  pid_t pid = -1;

  if (filesys_open(arg)) {
    // implementation of process_execute in argument passing branch, not here.
    // need to merge changes
    pid = process_execute(file);
  }
  return pid;
}

int wait (pid_t pid) {
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size) {
  if (file == NULL || !is_vaddr(file)) {
    exit(-1);
  }
  // Currently passess all tests for create but needs to be checked
  //all checks complete. create file
  return filesys_create(file, initial_size);
}

// reads num_args arguments from esp, stores in args
void read_args(void* esp, int num_args, void **args) {
  int i = 0;
  void *sp = esp;
  for (; i < num_args; i++) {
    sp += 4;
    if (!sp || !is_user_vaddr(sp)) {
      exit(-1);
    }
    args[i] = sp;
  }
}

bool remove (const char *file) {
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

int open (const char *file UNUSED) {
  return 0;  
}

int filesize (int fd UNUSED) {
  return 0;
}

int read (int fd UNUSED, void *buffer UNUSED, unsigned length UNUSED) {
  return 0;
}

void seek (int fd UNUSED, unsigned position UNUSED) {
  return;   
}

unsigned tell (int fd UNUSED) {
  return 0;
}

void close (int fd UNUSED) {
  return;
}

int write (int fd, const void *buffer, unsigned length) {
  // If no bytes have been written return default of 0
  int ret = 0;

  if (fd == STDOUT_FILENO) {
    // Writing to the console
    // returns number of bytes written
    /* TODO: Keep in mind edge case where number of bytes written
    is less than size because some were not able to be written. 
    */
    putbuf(buffer, length);
    ret = length;
  } 
  // TODO: Writing to an actual file
  return ret;
}

//should we make argument to this function void* or const char *?
void check_fp_valid(void* file) {
  //check if pointer to file passed in is valid
  if ((file) || (!is_vaddr(file))) {
    exit(-1);
  }
  if (!pagedir_get_page(thread_current()->pagedir, file)) {
    free(file);
    exit(-1);
  }
}

void try_acquiring_filesys() {
  if (lock_filesys.holder != thread_current()) {
    lock_acquire(&lock_filesys);
  }
}

void try_releasing_filesys() {
  if (lock_filesys.holder != thread_current()) {
    lock_release(&lock_filesys);
  }
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
  if (!is_user_vaddr(uaddr))
    return false;
  return pagedir_get_page(thread_current()->pagedir, uaddr) != NULL;
}