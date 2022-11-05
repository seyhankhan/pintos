#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int system_call_number = *(int*)(f->esp);

  switch (system_call_number) {
    case SYS_HALT:
    halt();
    break;

  


  }

  //printf ("system call!\n");
  //thread_exit ();
}

void halt(void) {
  shutdown_power_off();
}

void exit (int status) {
  return;
}

pid_t exec (const char *file) {
  return 0;
}

int wait (pid_t) {
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
  return 0;
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
