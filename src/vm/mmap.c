#include "mmap.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include <hash.h>
#include <list.h>

static struct lock lock_on_memory_file;
static struct hash memory_files_hash;

void insert_memory_file (mapid_t mapid, int fid, void *start_addr, void *end_addr) {
  struct memory_file *memory_file = (struct memory_file *) malloc (sizeof (struct memory_file));
  memory_file->fid = fid;
  memory_file->mapid = mapid;
  memory_file->start_addr = start_addr;
  memory_file->end_addr = end_addr;

  lock_acquire(&lock_on_memory_file);
  list_push_back(&thread_current()->memory_mapped_files, &memory_file->thread_elem);
  hash_insert(&memory_files_hash, &memory_file->hash_elem);
  lock_release(&lock_on_memory_file);
}

struct memory_file *find_memory_file(mapid_t mapid) {
  struct memory_file memory_file;
  struct hash_elem *e;

  memory_file.mapid = mapid;
  e = hash_find (&memory_files_hash, &memory_file.hash_elem);

  if (e != NULL) {
    return hash_entry (e, struct memory_file, hash_elem);
  }
  return NULL;
}

bool delete_memory_file(mapid_t mapid) {
  struct memory_file *memory_file = find_memory_file(mapid);
  if (memory_file == NULL)
    return false;

  lock_acquire(&lock_on_memory_file);
  hash_delete(&memory_files_hash, &memory_file->hash_elem);
  list_remove(&memory_file->thread_elem);
  free(memory_file);
  lock_release(&lock_on_memory_file);

  return true; 
}