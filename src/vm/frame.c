#include <stddef.h>
#include <stdio.h>

#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"

/* Frame table and locks*/
static struct hash frame_table;
static struct lock lock_on_frame;
static struct lock lock_eviction;

static struct list frames_for_eviction;
static struct list_elem *victim_elem;

static struct frame *evict_frame(void);
static struct frame *get_next_frame_for_eviction(void);
static void eviction_move_next(void);
static bool remove_frame_from_table(void *page_to_delete);
static struct frame *insert_frame_into_table(void *page_to_insert);


// static bool look_through_flip_if_necessary(struct frame *vf);

//compares keys in both hash_elems
bool less_compare_function(const struct hash_elem *first_hash_elem, const struct hash_elem *second_hash_elem, void *aux UNUSED) {
  const struct frame *first = hash_entry(first_hash_elem, struct frame, hash_elem);
  const struct frame *second = hash_entry(second_hash_elem, struct frame, hash_elem);
  return first->kpage < second->kpage;
}

//returns hash of hash_element's data
unsigned hashing_function(const struct hash_elem *hash_element, void *aux UNUSED) {
  const struct frame *frame = hash_entry(hash_element, struct frame, hash_elem);
  return hash_int((unsigned)frame->kpage);
}

/* Initialise frame table */
void initialise_frame() {
  lock_init(&lock_on_frame);
  lock_init(&lock_eviction);
  hash_init(&frame_table, hashing_function, less_compare_function, NULL);
  list_init(&frames_for_eviction);
  victim_elem = list_begin(&frames_for_eviction);
}

/* Removes frame from table and frees the struct and frees the kpage*/
void free_frame_from_table(void* page_to_free) {
  remove_frame_from_table(page_to_free);
  palloc_free_page(page_to_free);
}

/* Gets a free frame, if no free frames available evicts frame and returns free frame */
void *get_free_frame(struct spt_entry entry) {
  void *free_page_to_obtain = palloc_get_page(PAL_USER);
  if (free_page_to_obtain != NULL) {
    struct frame *f = insert_frame_into_table(free_page_to_obtain);
    f->pagedir = thread_current()->pagedir;
    f->upage = entry.upage;
    return f->kpage;
  } 
  // exit(-1);
  // NEED TO IMPLEMENT EVICTION STRATEGY HERE
  
  // At the moment will never reach here
  struct frame *f = evict_frame();
  f->pagedir = thread_current()->pagedir;
  f->upage = entry.upage;
  // printf("returned a frame with addr: %p\n", f->upage);
  return f->kpage;
  // return get_free_frame(flags);
  //PANIC("need to implement eviction");
}


/* Helper functions for fetching, inserting and removing frames from table*/
//add page to frame table
//returns true on success
static struct frame *insert_frame_into_table(void *page_to_insert) {
  struct frame *frame = (struct frame *) malloc(sizeof (struct frame));
  if (frame == NULL) {
    return NULL;
  }  
  lock_acquire(&lock_on_frame);
  hash_insert(&frame_table, &frame->hash_elem);
  list_push_back(&frames_for_eviction, &frame->list_elem);
  lock_release(&lock_on_frame); 
  frame->kpage = page_to_insert;
  frame->is_pinned = false;
  return frame;
}

//retrieve page from frame table
struct frame *get_frame_from_table(void *page_to_retrieve) {
  struct frame frame;
  frame.kpage = page_to_retrieve;
  struct hash_elem *hash_element = hash_find(&frame_table, &frame.hash_elem);
  if (hash_element != NULL) {
    return hash_entry(hash_element, struct frame, hash_elem);
  }
  return NULL;
}

//remove page from frame table
//returns true on success
static bool remove_frame_from_table(void *page_to_delete) {
  struct frame *frame = get_frame_from_table(page_to_delete);
  if (frame == NULL)
    return false;
  lock_acquire(&lock_on_frame);
  hash_delete(&frame_table, &frame->hash_elem);
  free(frame);
  lock_release(&lock_on_frame);
  return true;
}


//clock second chance variant. circular linked list
static struct frame *evict_frame() {
  // printf("Evicting frame\n");
  struct frame *frame_to_be_evicted = NULL;
  
  lock_acquire(&lock_on_frame);
  lock_acquire(&lock_eviction);

  if (frame_to_be_evicted == NULL) {
    // printf("Frame to be evicted is NULL\n");
		struct frame *frame = get_next_frame_for_eviction();
    ASSERT (frame != NULL);
    frame_to_be_evicted = frame;
    // printf("Found a frame\n");
  }
  lock_release(&lock_on_frame);
  lock_release(&lock_eviction);
  // printf("Freeing frame from table: %p\n", frame_to_be_evicted->kpage);
  struct spt_entry *entry = spt_find_addr(frame_to_be_evicted->upage);
  /* If entry is read only - don't write to swap just evict - file can be read again*/
  /* If entry is mmap - write back to file - don't write to swap */
  /* If entry is zero page (zero-bytes = PGSIZE) - don't write to swap)*/
  /* If page is dirty, swap out and add reference to spte that it was dirty to set it again when swapping in*/
  if (pagedir_is_dirty(frame_to_be_evicted->pagedir, entry->upage)){
    if (entry->is_mmap) {
      /* If it has been modified then write the modified page back to the file*/
      // printf("munmap\n");
      filesys_lock_acquire();
      file_seek(entry->file, entry->ofs);
      file_write(entry->file, entry->upage, entry->read_bytes);
      filesys_lock_release();
    } else {
      // printf("Swapping dirty page\n");
      entry->swap_index = swap_out(frame_to_be_evicted->kpage);
      entry->is_swapped = true;
    }
  }
  // printf("Clearing page\n");
  pagedir_clear_page(frame_to_be_evicted->pagedir, entry->upage);
  return frame_to_be_evicted;
}


/* Eviction Functions*/

static struct frame *get_next_frame_for_eviction() {
  // printf("Evicting\n");
	if (victim_elem == NULL || victim_elem == list_end(&frames_for_eviction)) {
  	victim_elem = list_begin(&frames_for_eviction);
  }
  //get frame struct from frame list
  struct frame *victim_frame;
  bool found = false;
  while (!found) {
    victim_frame = list_entry(victim_elem, struct frame, list_elem);
    if (victim_frame->is_pinned) {
      eviction_move_next();
      continue;
    }
    // printf("Upage: %pd\n",victim_frame->upage);
    if (pagedir_is_accessed(victim_frame->pagedir, victim_frame->upage)) {
      pagedir_set_accessed(victim_frame->pagedir, victim_frame->upage, false);
      eviction_move_next();
      continue;
    }
    found = true;
  }
  // return victim_frame;
  return victim_frame;
}

/* move position of next in frames_for_eviction */
static void eviction_move_next() {
  victim_elem = list_next(victim_elem);
  if (victim_elem == NULL || victim_elem == list_end(&frames_for_eviction)) {
    // printf("Victim: %p\n", victim_elem);
    victim_elem = list_begin(&frames_for_eviction);
  } 
}

/* Algorithm
1. We have a circular list of frames with all their reference bits initialised to 0
2. Have a pointer to a victim frame initially - arbitrary - can use the first element
3. On a page fault, when accessing a page we load it into a free frame and set the reference bit to 1
4. If there are no more free frames left then we start the eviction algorithm
5. We begin with the victim frame. If victim frame has a reference bit set to 0 
   then we swap_out the page the frame currently holds 
6. save the swap index in the spt_entry for that page that was swapped out and set the is_swapped bool to true
7. Return the frame that was freed to be allocated to the page that originally faulted and repeat step 3
8. If a page fault occurs on a page that's currently swapped then use swap_in passing the spt_entries swap index
  and the kpage of the frame you want to load it to.

Note: 
If a page has been selected for eviction but is dirty then we pin it and move to the next
If a file is read-only (writable == false) then don't write to swap as it can be loaded again from file

*/





