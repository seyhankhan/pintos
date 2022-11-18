#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

//created when a process creates a child. Only deallocated when both the parent and child terminate
struct process_exit_status {
  //exit code, 
  int exit_code; //shared by parent and child
  int ref_count; //keeps track of parent and child being alive to determine when to allocate. shared by parent and child
  int child_pid; //used by parent to find correct process_exit_status among its children
  bool loaded; //process loaded correctly? parent&child
  //int parent_pid maybe neccesary
  struct list_elem elem; //for parent threads list of process
  struct semaphore sema;
  struct lock lock;

  //add a semaphore/lock to ensure thread safety when changing this data?
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

void dec_ref_count(struct process_exit_status *exit_status);

#endif /* userprog/process.h */
