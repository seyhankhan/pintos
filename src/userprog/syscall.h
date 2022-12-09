#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "vm/mmap.h"

void syscall_init (void);

void exit(int status);
void munmap(mapid_t mapping);

void filesys_lock_acquire(void);
void filesys_lock_release(void);
bool try_filesys_lock_acquire(void);
bool try_filesys_lock_release(void);


#endif /* userprog/syscall.h */
