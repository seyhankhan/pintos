#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "vm/mmap.h"

void syscall_init (void);

void exit(int status);
void munmap(mapid_t mapping);

#endif /* userprog/syscall.h */
