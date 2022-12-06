#include "mmap.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include <hash.h>
#include <list.h>

static struct lock lock_on_memory_file;
static struct hash memory_files_hash;

void insert_mfile (mapid_t mapid, int fid, void *start_addr, void *end_addr) {
  struct memory_file *f = (struct memory_file *) malloc (sizeof (struct memory_file));
  f->fid = fid;
  f->mapid = mapid;
  f->start_addr = start_addr;
  f->end_addr = end_addr;

  lock_acquire(&lock_on_memory_file);
  list_push_back(&thread_current()->memory_mapped_files, &f->thread_elem);
  hash_insert(&memory_files_hash, &f->hash_elem);
  lock_release(&lock_on_memory_file);
}