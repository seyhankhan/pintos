#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

//Stores the status of a process, and synchronization structures to ensure thread safe access
struct process_exit_status {
  //exit code, 
  int exit_code; //shared by parent and child
  int ref_count; //keeps track of parent and child being alive to determine when to allocate. shared by parent and child
  int child_pid; //used by parent to find correct process_exit_status among its children
  bool loaded;   //stores whether the process loaded correctly. shared by parent and child
  
  struct list_elem elem; //for parent thread's list of process statuses
  struct semaphore sema; //downed by process_wait and upped by process_exit to ensure a parent waits for its child
  struct lock lock;      //used for thread safe access to the shared data

  //add a semaphore/lock to ensure thread safety when changing this data?
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

//decrements the ref_count of exit_status and frees the memory if no more references exist
void dec_ref_count(struct process_exit_status *exit_status);

#endif /* userprog/process.h */
