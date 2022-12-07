#include "page.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include <string.h>


static struct hash shared_pages;

static unsigned shared_hash_func (const struct hash_elem *e, void *aux UNUSED) {
   struct spt_entry *spte = hash_entry(e, struct spt_entry, hash_elem);
   return hash_int((int) pg_round_down(spte->upage) ^ (int) spte->ofs);
}

static bool shared_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  return pg_round_down(hash_entry(a, struct spt_entry, hash_elem)->upage) 
        < pg_round_down(hash_entry(b, struct spt_entry, hash_elem)->upage);
}

void init_shared_pages(void) {
   hash_init(&shared_pages, shared_hash_func, shared_less_func, NULL);
}

void insert_shared_page(struct spt_entry *spt_entry) {
   hash_insert(&shared_pages, &spt_entry->hash_elem);
}


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

/*bool lazy_load_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) {*/
bool lazy_load_page(struct spt_entry *spt_entry) {

   ASSERT(pg_ofs (spt_entry->upage) == 0);
   ASSERT(spt_entry->ofs % PGSIZE == 0);

   file_seek (spt_entry->file, spt_entry->ofs);

   size_t page_read_bytes = spt_entry->read_bytes < PGSIZE ? spt_entry->read_bytes : PGSIZE;
   size_t page_zero_bytes = PGSIZE - page_read_bytes;
   
   struct hash_elem * elem = hash_find(&shared_pages, &spt_entry->hash_elem);
   if (elem != NULL) {
      struct spt_entry *spt_other = hash_entry(elem, struct spt_entry, hash_elem);
      uint8_t *kpage = pagedir_get_page (spt_other->page_dir, spt_other->upage);

      if (!pagedir_set_page(spt_entry->page_dir, spt_entry->upage, kpage, spt_entry->writable)) 
      {
         free_frame_from_table(kpage);
         return false; 
      }     
   } else {
       // Check if virtual page already allocated 
      struct thread *t = thread_current ();
      uint8_t *kpage = pagedir_get_page (spt_entry->page_dir, spt_entry->upage);
      //uint8_t *kpage = obtain_free_frame(PAL_USER);
      if (kpage == NULL){
         
         // Get a new page of memory.
         kpage = obtain_free_frame(PAL_USER);
         if (kpage == NULL){
            return false;
         }
         
         // Add the page to the process's address space. 
         if (!pagedir_set_page(t->pagedir, spt_entry->upage, kpage, spt_entry->writable)) 
         {
            free_frame_from_table(kpage);
            return false; 
         }     
         if (!spt_entry->writable) {
            insert_shared_page(spt_entry);
         }
         
      } else {
         
      //   Check if writable flag for the page should be updated 
         if(spt_entry->writable && !pagedir_is_writable(t->pagedir, spt_entry->upage)){
            pagedir_set_writable(t->pagedir, spt_entry->upage, spt_entry->writable); 
         }
         
      }

      //  Load data into the page. 
      if (file_read (spt_entry->file, kpage, page_read_bytes) != (int) page_read_bytes) {
         return false; 
      }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);
   }
   return true;
}