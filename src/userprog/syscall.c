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

static void syscall_handler (struct intr_frame *);
void read_args(void* esp, int num_args, void **args);
void check_fp_valid(void *file);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int system_call_number = *(int*)(f->esp);

  // array to store arguements passed into the system call
  // passed to relevant function too
  // void* args[3];
  // hex_dump((uintptr_t) f->esp, f->esp, PHYS_BASE - f->esp, true);
  int *esp = f->esp;

  esp++;

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
    case SYS_EXIT:
      thread_exit();
    break;
  }

  //printf ("system call!\n");
  //thread_exit ();
}

void halt(void) {
  shutdown_power_off();
}

void exit (int status) {
  thread_current()->exit_code = status;
  thread_exit();
}

pid_t exec (const char *file) {
  return 0;
}

int wait (pid_t  pid) {
  return 0;
}

bool create (const char *file, unsigned initial_size) {

  //check if pointer to file passed in is valid
  if ((file) || (!is_user_vaddr(file))) {
    exit(-1);
  }
  if (!pagedir_get_page(thread_current()->pagedir, file)) {
    free(file);
    exit(-1);
  }

  //check if file has no name
  if (!strcmp(file, "")) {
    return false;
  }

  //all checks complete. create file
  return filesys_create(file, initial_size);
}

bool remove (const char *file) {
  return 0;
}

int open (const char *file) {
  return 0;  
}

int filesize (int fd) {
  return 0;
}

int read (int fd, void *buffer, unsigned length) {
  return 0;
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

void seek (int fd, unsigned position) {
  return;   
}

unsigned tell (int fd) {
  return 0;
}

void close (int fd) {
  return;
}


void halt(void) {
  shutdown_power_off();
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

//wait(pid) return process_wait(pid);


// pid_t exec (const char *file) {
//   check_fp_valid(file);

//   char *arg;
//   char *save_ptr;

//   int filename_length = strlen(file);
//   char filename_copy[filename_length];
//   strlcpy(filename_copy, file, filename_length);
//   arg = strtok_r(filename_copy, " ", &save_ptr);

//   pid_t pid = -1;

//   if (filesys_open(arg)) {
//     // implementation of process_execute in argument passing branch, not here.
//     // need to merge changes
//     pid = process_execute(file);
//   }
//   return pid;
// }
