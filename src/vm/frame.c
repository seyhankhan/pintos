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
static struct list_elem *next;

static void evict_frame(void);
static struct frame *get_next_frame_for_eviction(void);
static void eviction_move_next(void);
static void evict_frame(void);
static bool remove_frame_from_table(void *page_to_delete);
static bool insert_frame_into_table(void *page_to_insert);
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
}

/* Removes frame from table and frees the struct and frees the kpage*/
void free_frame_from_table(void* page_to_free) {
  remove_frame_from_table(page_to_free);
  palloc_free_page(page_to_free);
}

/* Gets a free frame, if no free frames available evicts frame and returns free frame */
void *get_free_frame(enum palloc_flags flags) {
  void *free_page_to_obtain = palloc_get_page(flags);
  if (free_page_to_obtain != NULL) {
    insert_frame_into_table(free_page_to_obtain);
    get_frame_from_table(free_page_to_obtain);
  } else {
    exit(-1);
    // NEED TO IMPLEMENT EVICTION STRATEGY HERE
    
    // At the moment will never reach here
    evict_frame();
    // return get_free_frame(flags);
    //PANIC("need to implement eviction");
  }
  return free_page_to_obtain;
}


/* Helper functions for fetching, inserting and removing frames from table*/
//add page to frame table
//returns true on success
static bool insert_frame_into_table(void *page_to_insert) {
  struct frame *frame = (struct frame *) malloc(sizeof (struct frame));
  if (frame == NULL) {
    return false;
  }  
  lock_acquire(&lock_on_frame);
  hash_insert(&frame_table, &frame->hash_elem);
  lock_release(&lock_on_frame); 
  frame->kpage = page_to_insert;
  return true;
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
static void evict_frame() {
  printf("Evicting frame\n");
  struct frame *frame_to_be_evicted = NULL;
  
  lock_acquire(&lock_on_frame);
  lock_acquire(&lock_eviction);

  while (frame_to_be_evicted == NULL) {
    printf("Frame to be evicted is NULL\n");
		struct frame *frame = get_next_frame_for_eviction();
    ASSERT (frame != NULL);
    //move on id frame is pinned or accessed
    if (frame->is_pinned /* Or accessed */) {
      eviction_move_next();
  	  continue;  
    }    
    frame_to_be_evicted = frame;
    printf("Found a frame\n");
  }
  lock_release(&lock_on_frame);
  lock_release(&lock_eviction);
  printf("Freeing frame from table: %p\n", frame_to_be_evicted->kpage);
  struct spt_entry *entry = spt_find_addr(frame_to_be_evicted->upage);
  entry->swap_index = swap_out(frame_to_be_evicted->upage);
}


/* Eviction Functions*/

static struct frame *get_next_frame_for_eviction() {
	if (next == NULL || next == list_end(&frames_for_eviction)) {
  	next = list_begin(&frames_for_eviction);
  }
  //get frame struct from frame list
  struct frame *vf = list_entry(next, struct frame, list_elem);
  return vf;
}

/* move position of next in frames_for_eviction */
static void eviction_move_next() {
  if (next == NULL || next == list_end(&frames_for_eviction)) {
    next = list_begin(&frames_for_eviction);
  } else {
    next = list_next(next);
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


//go through all pages sharing the frame, inspect accessed bit of each page
//if all 0, evict this frame
//otherwise, flip first bit to 1
// static bool look_through_flip_if_necessary(struct frame *vf) {
//   struct list_elem *e;
//   for (e = list_begin(&vf->pages); e != list_end(&vf->pages); e = list_next(e)) {
//     struct page *page = list_entry(e, struct page, frame_elem);
//     if (pagedir_is_accessed(&page->pagedir, page->addr)) {
//       pagedir_set_accessed(&page->pagedir, page->addr, false);
//       return false;
//     }
//   }
//   return true;
// }




