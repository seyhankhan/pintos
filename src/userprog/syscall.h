#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

typedef int pid_t;

void syscall_init (void);

void try_acquiring_filesys(void);
void try_releasing_filesys(void);
void read_args(void* esp, int num_args, void **args);
void check_fp_valid(void* file);

struct file_wrapper {
    struct file *file;
    struct list_elem file_elem;
    int fd;
};

#endif /* userprog/syscall.h */
