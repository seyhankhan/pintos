#include "mmap.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "lib/kernel/list.h"

static struct lock lock_on_memory_file;

void init_mmap() {
  lock_init(&lock_on_memory_file); 
}

void insert_mfile (mapid_t mapid, struct file *file, void* start_addr, void* end_addr) {
  struct memory_file *memory_file = (struct memory_file *) malloc (sizeof (struct memory_file));
  memory_file->file = file;
  memory_file->mapid = mapid;
  memory_file->start_addr = start_addr;
  memory_file->end_addr = end_addr;
  list_push_back(&thread_current()->memory_mapped_files, &memory_file->elem);
}

void delete_mfile(struct memory_file *mfile) {
  list_remove(&mfile->elem);
  free(mfile);
}

struct memory_file *get_mfile(mapid_t mapid) {
  struct list_elem *elem;
  elem = list_begin (&thread_current()->memory_mapped_files);
  while (elem != list_end (&thread_current()->memory_mapped_files)) {
    struct memory_file *mfile = list_entry (elem, struct memory_file, elem);
    if (mfile->mapid == mapid) {
      return mfile;
    }
    elem = list_next (elem);
  }
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

