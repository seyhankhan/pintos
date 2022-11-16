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

static void syscall_handler (struct intr_frame *);
void read_args(void* esp, int num_args, void **args);
void try_acquiring_filesys(void);
void try_releasing_filesys(void);

//should we make argument to this function void* or const char *?
void check_fp_valid(void *file);

struct lock lock_filesys;


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&lock_filesys);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
    int system_call_number = *(int*)(f->esp);

    // array to store arguements passed into the system call
    // passed to relevant function too
    void* args[3];

  // array to store arguements passed into the system call
  // passed to relevant function too
  // void* args[3];
  // hex_dump((uintptr_t) f->esp, f->esp, PHYS_BASE - f->esp, true);
  int *esp = f->esp;

  esp++;
  // Getting the arguments should be done in a better way
  int arg1 = *esp;
  int arg2 = *(esp + 1);
  int arg3 = *(esp + 2);
  
  switch (system_call_number) {
    case SYS_HALT:
      halt();
    break;
    case SYS_WRITE:
      // printf("Called Write\n");
      f->eax =write(arg1, (void * ) arg2, arg3);
    break;
    // Need to properly deal with exiting
    case SYS_EXIT:
      exit(0);
    break;
    case SYS_REMOVE:
      read_args(f->esp, 1, args);
      try_acquiring_filesys();
      f->eax = remove(*(char**)args[0]);
      try_releasing_filesys();
    break;
  }

  //printf ("system call!\n");
  //thread_exit ();
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
  sema_up(&cur->exit_status->sema);
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

  check_fp_valid((char *) file);

  //check if file has no name
  if (!strcmp(file, "")) {
    return false;
  }

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
  bool deleted_file = filesys_remove(file);
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
  if ((file) || (!is_user_vaddr(file))) {
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
