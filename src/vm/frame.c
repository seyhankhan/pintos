#include <stddef.h>
#include <stdio.h>

#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"

static struct hash frame_table;
static struct lock lock_on_frame;
static struct lock lock_eviction;

static struct list frames_for_eviction;
static struct list_elem *next;

static void evict_frame(void);
static struct frame *get_next_frame_for_eviction(void);
static void eviction_move_next(void);
static bool look_through_flip_if_necessary(struct frame *vf);

//compares keys in both hash_elems
bool less_compare_function(const struct hash_elem *first_hash_elem, const struct hash_elem *second_hash_elem, void *aux UNUSED) {
  const struct frame *first = hash_entry(first_hash_elem, struct frame, hash_elem);
  const struct frame *second = hash_entry(second_hash_elem, struct frame, hash_elem);
  return first->frame_page_address < second->frame_page_address;
}

//returns hash of hash_element's data
unsigned hashing_function(const struct hash_elem *hash_element, void *aux UNUSED) {
  const struct frame *frame = hash_entry(hash_element, struct frame, hash_elem);
  return hash_int((unsigned)frame->frame_page_address);
}

/* Initialise frame table */
void initialise_frame() {
  lock_init(&lock_on_frame);
  hash_init(&frame_table, hashing_function, less_compare_function, NULL);
}

//add page to frame table
//returns true on success
static bool insert_page_into_frame(void *page_to_insert) {

  struct frame *frame = (struct frame *) malloc(sizeof (struct frame));
  if (frame == NULL) {
    return false;
  }  
  lock_acquire(&lock_on_frame);
  hash_insert(&frame_table, &frame->hash_elem);
  lock_release(&lock_on_frame); 
  frame->page = page_to_insert;
  frame->thread = thread_current();

  return true;
}

//retrieve page from frame table
struct frame *retrieve_page_from_frame(void *page_to_retrieve) {
  struct frame frame;
  frame.page = page_to_retrieve;
  struct hash_elem *hash_element = hash_find(&frame_table, &frame.hash_elem);
  if (hash_element != NULL) {
    return hash_entry(hash_element, struct frame, hash_elem);
  }
  return NULL;
}

//remove page from frame table
//returns true on success
static bool remove_page_from_frame(void *page_to_delete) {
  struct frame *frame = retrieve_page_from_frame(page_to_delete);
  if (frame == NULL)
    return false;
  lock_acquire(&lock_on_frame);
  hash_delete(&frame_table, &frame->hash_elem);
  free(frame);
  lock_release(&lock_on_frame);

  return true;
}

//gets a free frame from the frame table
//still need to implement our eviction strategy
void *obtain_free_frame(enum palloc_flags flags) {
  void *free_page_to_obtain = palloc_get_page(flags);

  if (free_page_to_obtain != NULL) {
    insert_page_into_frame(free_page_to_obtain);
    retrieve_page_from_frame(free_page_to_obtain);
  } else {
    #ifdef VM
    exit(-1);
    #endif

    // NEED TO IMPLEMENT EVICTION STRATEGY HERE
    
    evict_frame();
    
    //PANIC("need to implement eviction");
  }
  return free_page_to_obtain;
}

//clock second chance variant. circular linked list
static void evict_frame() {
  struct frame *frame_to_be_evicted = NULL;
  
  lock_acquire(&lock_on_frame);
  lock_acquire(&lock_eviction);

  while (frame_to_be_evicted == NULL) {
		struct frame *frame = get_next_frame_for_eviction();
    ASSERT (frame != NULL);

    //move on id frame is pinned or accessed
    if (frame->is_pinned == true || look_through_flip_if_necessary(frame) == false) {
      eviction_move_next();
  	  continue;  
    }    
    frame_to_be_evicted = frame;
  }

  lock_release(&lock_on_frame);
  lock_release(&lock_eviction);

  //need to free everything
  
}

static struct frame *get_next_frame_for_eviction() {
	if (next == NULL || next == list_end(&frames_for_eviction)) {
  	next = list_begin(&frames_for_eviction);
  }  
  if (next != NULL) {
    //get frame struct from frame list
		struct frame *vf = list_entry(next, struct frame, list_elem);
    return vf;
  }  
  //shouldn't reach here

}


//move position of next in frames_for_eviction
static void eviction_move_next() {
  if (next == NULL || next == list_end(&frames_for_eviction)) {
    next = list_begin(&frames_for_eviction);
  } else {
    next = list_next(next); 
  }
}

//go through all pages sharing the frame, inspect accessed bit of each page
//if all 0, evict this frame
//otherwise, flip first bit to 1

static bool look_through_flip_if_necessary(struct frame *vf) {
  struct list_elem *e;
  
  for (e = list_begin(&vf->pages); e != list_end(&vf->pages); e = list_next(e)) {
    struct page *page = list_entry(e, struct page, frame_elem);
    if (pagedir_is_accessed(&page->pagedir, page->addr)) {
      pagedir_set_accessed(&page->pagedir, page->addr, false);
      return false;
    }
  }
  return true;
}


void free_frame_from_table(void* page_to_free) {
  remove_page_from_frame(page_to_free);
  palloc_free_page(page_to_free);
}

//maps a user virtual page to the frame by setting relevant fields of frame struct
//returns true on success
bool map_user_vp_to_frame(void *page, uint32_t *page_table_entry, void *frame_page_address) {
  struct frame *frame = retrieve_page_from_frame(page);
  if (frame == NULL) {
    return false;
  }
  frame->frame_page_address = frame_page_address;
  frame->page_table_entry = page_table_entry;
  return true;
}