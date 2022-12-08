#include "mmap.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "lib/kernel/list.h"

static struct lock mfile_lock;

void init_mmap() {
  lock_init(&mfile_lock); 
}

void insert_mfile (mapid_t mapid, struct file *file, void* start_addr, void* end_addr) {
  struct memory_file *memory_file = (struct memory_file *) malloc (sizeof (struct memory_file));
  memory_file->file = file;
  memory_file->mapid = mapid;
  memory_file->start_addr = start_addr;
  memory_file->end_addr = end_addr;
  lock_acquire(&mfile_lock);
  list_push_back(&thread_current()->memory_mapped_files, &memory_file->elem);
  lock_release  (&mfile_lock);
}

void delete_mfile(struct memory_file *mfile) {
  lock_acquire(&mfile_lock);
  list_remove(&mfile->elem);
  lock_release  (&mfile_lock);
  free(mfile);
}

struct memory_file *get_mfile(mapid_t mapid) {
  struct list_elem *elem;
  lock_acquire(&mfile_lock);
  elem = list_begin (&thread_current()->memory_mapped_files);
  while (elem != list_end (&thread_current()->memory_mapped_files)) {
    struct memory_file *mfile = list_entry (elem, struct memory_file, elem);
    if (mfile->mapid == mapid) {
      lock_release  (&mfile_lock);
      return mfile;
    }
    elem = list_next (elem);
  }
  lock_release(&mfile_lock);
  return NULL;
}


