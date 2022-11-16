#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

typedef int pid_t;

void syscall_init (void);

void try_acquiring_filesys(void);
void try_releasing_filesys(void);
void read_args(void* esp, int num_args, void **args);
void check_fp_valid(void* file);

#endif /* userprog/syscall.h */
