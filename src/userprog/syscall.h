#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

typedef int pid_t;

void syscall_init (void);

void halt(void); 
void exit (int status);
pid_t exec (const char *file);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

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
