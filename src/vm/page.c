#include "page.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include <string.h>


unsigned hash_func (const struct hash_elem *e, void *aux UNUSED) 
{
    struct spt_entry *spte = hash_entry(e, struct spt_entry, hash_elem);
    return hash_int((int) pg_round_down(spte->upage));
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  return pg_round_down(hash_entry(a, struct spt_entry, hash_elem)->upage) 
        < pg_round_down(hash_entry(b, struct spt_entry, hash_elem)->upage);
}

struct spt_entry *spt_find_addr(const void *addr) {
  struct spt_entry entry;
  entry.upage = (void *) addr;
  struct hash_elem *elem = hash_find(&thread_current()->spt, &entry.hash_elem);

  if (elem != NULL) {
    return hash_entry(elem, struct spt_entry, hash_elem);
  } else {
    return NULL;
  }
}

bool lazy_load_page(struct spt_entry *entry) {

   ASSERT(pg_ofs (entry->upage) == 0);
   ASSERT(entry->ofs % PGSIZE == 0);

   file_seek (entry->file, entry->ofs);

   size_t page_read_bytes = entry->read_bytes < PGSIZE ? entry->read_bytes : PGSIZE;
   size_t page_zero_bytes = PGSIZE - page_read_bytes;
   

   // Check if virtual page already allocated 
   struct thread *t = thread_current ();
   uint8_t *kpage = pagedir_get_page (t->pagedir, entry->upage);
   if (kpage == NULL){
      
      // if (!load_page(entry))
      //    return false;
      // Get a new page of memory.
      kpage = obtain_free_frame(PAL_USER);
      if (kpage == NULL){
         // Ideally this won't be the case as we will evict frames to make space
         return false;
      }
      
      // Add the page to the process's address space. 
      if (!pagedir_set_page(t->pagedir, entry->upage, kpage, entry->writable)) {
         free_frame_from_table(kpage);
         return false; 
      }    
   } else {
      
   //   Check if writable flag for the page should be updated 
      if(entry->writable && !pagedir_is_writable(t->pagedir, entry->upage)){
         pagedir_set_writable(t->pagedir, entry->upage, entry->writable); 
      }
      
   }
   //  Load data into the page. 
   if (file_read (entry->file, kpage, page_read_bytes) != (int) page_read_bytes) {
      return false; 
   }

   // Fill the rest of the page with zeros
   memset (kpage + page_read_bytes, 0, page_zero_bytes);

   return true;
}

struct spt_entry *spt_add_page(struct hash *spt, struct spt_entry *entry) {
  struct hash_elem *old = hash_insert(spt, &entry->hash_elem);
  if (old != NULL) {
    return hash_entry(old, struct spt_entry, hash_elem);
  }
  return NULL;
}

struct spt_entry *create_file_page(struct file *file, void *upage,  
                                    off_t ofs, size_t read_bytes,
                                    size_t zero_bytes, bool writable) {
   struct spt_entry *page = malloc(sizeof(struct spt_entry));

   if (page == NULL)
      return NULL;
   
   page->file = file;
   page->upage = upage;
   page->ofs = ofs;
   page->read_bytes = read_bytes;
   page->zero_bytes = zero_bytes;
   page->writable = writable;

   return page;
}

struct spt_entry *create_zero_page(void *addr, bool writable) {
      struct spt_entry *page = malloc(sizeof(struct spt_entry));

   if (page == NULL)
      return NULL;
   
   page->file = NULL;
   page->upage = addr;
   page->ofs = 0;
   page->zero_bytes = PGSIZE;
   page->writable = writable;

   return page;
}

bool load_page(struct spt_entry *page) {
   // Get a new page of memory.
   uint8_t *kpage = obtain_free_frame(PAL_USER);
   if (kpage == NULL){
      // Ideally this won't be the case as we will evict frames to make space
      return false;
   }
   // Add the page to the process's address space. 
   if (!pagedir_set_page(thread_current()->pagedir, page->upage, kpage, page->writable)) {
      free_frame_from_table(kpage);
      return false; 
   } 

   return true;
}