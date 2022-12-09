#include "page.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include <string.h>
#include "threads/malloc.h"
#include "hash.h"
#include "userprog/syscall.h"
#include "devices/swap.h"

static struct lock spt_lock;

void init_page_lock() {
   lock_init(&spt_lock);
}

unsigned hash_func(const struct hash_elem *e, void *aux UNUSED) {
   struct spt_entry *spte = hash_entry(e, struct spt_entry, hash_elem);
   return hash_int((int)pg_round_down(spte->upage));
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
   return pg_round_down(hash_entry(a, struct spt_entry, hash_elem)->upage) < pg_round_down(hash_entry(b, struct spt_entry, hash_elem)->upage);
}

struct spt_entry *spt_find_addr(const void *addr) {
   // lock_acquire(&spt_lock);
   struct spt_entry entry;
   entry.upage = (void *)addr;
   struct hash_elem *elem = hash_find(&thread_current()->spt, &entry.hash_elem);
   // lock_release(&spt_lock);
   if (elem != NULL) {
      return hash_entry(elem, struct spt_entry, hash_elem);
   }
   return NULL;

}

bool load_page_from_spt(struct spt_entry *entry) {

   ASSERT(pg_ofs (entry->upage) == 0);
   ASSERT(entry->ofs % PGSIZE == 0);
   // lock_acquire(&spt_lock);

   size_t page_read_bytes = entry->read_bytes < PGSIZE ? entry->read_bytes : PGSIZE;
   size_t page_zero_bytes = PGSIZE - page_read_bytes;
   
   // Check if virtual page already allocated 
   struct thread *t = thread_current ();
   uint8_t *kpage = pagedir_get_page (t->pagedir, entry->upage);
   if (kpage == NULL){
      // Get a new page of memory.
      kpage = get_free_frame(PAL_USER);
      if (kpage == NULL)
      {
         // Ideally this won't be the case as we will evict frames to make space
         // lock_release(&spt_lock);
         return false;
      }
      /* If entry is swapped and (mmap or read only) then read back from file*/
      /* If entry is zero page create zero page - when eviciting don't write to swap*/
      /* If entry is swapped and file that was dirty,set_dirty bit again - need to store it for spte*/
      // Add the page to the process's address space.
      if (!pagedir_set_page(t->pagedir, entry->upage, kpage, entry->writable)) {
         free_frame_from_table(kpage);
         // lock_release(&spt_lock);
         return false;
      }
   } else {
      //   Check if writable flag for the page should be updated
      if (entry->writable && !pagedir_is_writable(t->pagedir, entry->upage)) {
         pagedir_set_writable(t->pagedir, entry->upage, entry->writable);
      }
   }
   //  Load data into the page.
   if (entry->zero_bytes == PGSIZE) {
      memset(kpage, 0, page_zero_bytes);
   } else {
      bool release = try_filesys_lock_acquire(); 
      file_seek(entry->file, entry->ofs);
      if (file_read(entry->file, kpage, page_read_bytes) != (int)page_read_bytes) {
         if (release) {
            filesys_lock_release();
         }
         return false;
      }
      if (release) {
         filesys_lock_release();
      }
      memset(kpage + page_read_bytes, 0, page_zero_bytes);
   }
   // lock_release(&spt_lock);

   pagedir_set_dirty (thread_current()->pagedir, entry->upage, false);
   pagedir_set_accessed (thread_current()->pagedir, entry->upage, true);

   return true;
}

struct spt_entry *spt_add_page(struct hash *spt, struct spt_entry *entry) {  
   // lock_acquire(&spt_lock);
   struct hash_elem *old = hash_insert(spt, &entry->hash_elem);
   // lock_release(&spt_lock);
   if (old != NULL)
   {
      return hash_entry(old, struct spt_entry, hash_elem);
   }
   return NULL;
}

bool spt_delete_page(struct hash *spt, void *page)
{
   struct spt_entry *e = spt_find_addr(page);
   if (!e)
      return false;
   // lock_acquire(&spt_lock);
   struct hash_elem *he = hash_delete(spt, &e->hash_elem);
   // // lock_release(&spt_lock);
   if (!he)
   {
      return false;
   }
   return true;
}

struct spt_entry *create_file_page(struct file *file, void *upage,
                                   off_t ofs, size_t read_bytes,
                                   size_t zero_bytes, bool writable)
{
   struct spt_entry *page = malloc(sizeof(struct spt_entry));

   if (page == NULL)
      return NULL;

   page->file = file;
   page->upage = upage;
   page->ofs = ofs;
   page->read_bytes = read_bytes;
   page->zero_bytes = zero_bytes;
   page->writable = writable;
   page->is_swapped = false;
   return page;
}

struct spt_entry *create_zero_page(void *addr, bool writable)
{
   struct spt_entry *page = malloc(sizeof(struct spt_entry));

   if (page == NULL)
      return NULL;

   page->file = NULL;
   page->upage = addr;
   page->ofs = 0;
   page->read_bytes = 0;
   page->zero_bytes = PGSIZE;
   page->writable = writable;
   page->is_swapped = false;
   return page;
}