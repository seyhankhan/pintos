#include "page.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include <string.h>
#include "threads/malloc.h"

static struct list list_of_pages;
static struct lock lock_on_list;
static int count = 0;

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

bool lazy_load_page(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) {

   ASSERT(pg_ofs (upage) == 0);
   ASSERT(ofs % PGSIZE == 0);

   file_seek (file, ofs);

   size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
   size_t page_zero_bytes = PGSIZE - page_read_bytes;
   

   // Check if virtual page already allocated 
   struct thread *t = thread_current ();
   uint8_t *kpage = pagedir_get_page (t->pagedir, upage);
   //uint8_t *kpage = obtain_free_frame(PAL_USER);
   if (kpage == NULL){
      
      // Get a new page of memory.
      kpage = obtain_free_frame(PAL_USER);
      if (kpage == NULL){
         return false;
      }
      
      // Add the page to the process's address space. 
      if (!pagedir_set_page(t->pagedir, upage, kpage, writable)) 
      {
         free_frame_from_table(kpage);
         return false; 
      }     
      
   } else {
      
   //   Check if writable flag for the page should be updated 
      if(writable && !pagedir_is_writable(t->pagedir, upage)){
         pagedir_set_writable(t->pagedir, upage, writable); 
      }
      
   }

   //  Load data into the page. 
   if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
      return false; 
   }
   memset (kpage + page_read_bytes, 0, page_zero_bytes);

   //  Advance. 
   read_bytes -= page_read_bytes;
   zero_bytes -= page_zero_bytes;
   upage += PGSIZE;
   return true;
}

struct page* find_page(void *addr) {
  uint32_t *pagedir = thread_current ()->pagedir;
  struct list_elem *e;
  lock_acquire(&lock_on_list);

  for (e = list_begin (&list_of_pages); e != list_end (&list_of_pages);
       e = list_next(e)) {
      struct page *page = list_entry(e, struct page, list_elem);
      if (page->addr == addr && page->pagedir == pagedir)
        {
          lock_release (&lock_on_list);
          return page;
        }
    }
  lock_release (&lock_on_list);
  return NULL;
}

struct page* create_new_file_page(void *addr, struct file *file, off_t ofs, size_t read_bytes, size_t zero_bytes, bool can_write) {
   struct page* page = (struct page*) malloc (sizeof(struct page));
   if (page == NULL) {
      return NULL;
   }

   page->addr = addr;
   page->data->file = file;
   page->data->ofs = ofs;
   page->data->read_bytes = read_bytes;
   page->data->zero_bytes = zero_bytes;
   page->can_write = can_write;

   //add page to list of pages
   lock_acquire (&lock_on_list);
   list_push_back (&list_of_pages, &page->list_elem);
   lock_release (&lock_on_list);
   
   return page;
}  

bool delete_page(struct page *page) {
  lock_acquire (&lock_on_list);
  list_remove (&page->list_elem);
  free (page);
  --count;
  lock_release (&lock_on_list);
  return true;
}     

