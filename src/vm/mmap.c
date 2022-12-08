#include "mmap.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "lib/kernel/list.h"

static struct lock lock_on_memory_file;

void init_mmap() {
  lock_init(&lock_on_memory_file); 
}

void insert_mfile (mapid_t mapid, struct file *file, struct list *file_pages) {
  struct memory_file *memory_file = (struct memory_file *) malloc (sizeof (struct memory_file));
  memory_file->file = file;
  memory_file->mapid = mapid;
  memory_file->file_pages = file_pages;
  memory_file->num_of_pages = list_size(file_pages);
  lock_acquire(&lock_on_memory_file);
  list_push_back(&thread_current()->memory_mapped_files, &memory_file->elem);
  lock_release(&lock_on_memory_file);
}

void delete_mfile(struct memory_file *mfile) {
  lock_acquire(&lock_on_memory_file);
  list_remove(&mfile->elem);
  lock_release(&lock_on_memory_file);
  free(mfile);
}

struct memory_file *get_mfile(mapid_t mapid) {
  struct list_elem *elem;
  lock_acquire(&lock_on_memory_file);
  elem = list_begin (&thread_current()->memory_mapped_files);
  while (elem != list_end (&thread_current()->memory_mapped_files)) {
    struct memory_file *mfile = list_entry (elem, struct memory_file, elem);
    if (mfile->mapid == mapid) {
      lock_release(&lock_on_memory_file);
      return mfile;
    }
    elem = list_next (elem);
  }
  lock_release(&lock_on_memory_file);
  return NULL;
}

// unsigned hash_mfile (const struct hash_elem *e, void *aux UNUSED) {
//   return hash_int(hash_entry(e, struct memory_file, elem)->mapid);
// }
// bool hash_less_mfile (const struct hash_elem *a,
//                              const struct hash_elem *b,
//                              void *aux UNUSED) {
//   return hash_entry(a, struct memory_file, elem)->mapid <
//          hash_entry(b, struct memory_file, elem)->mapid;
// }

