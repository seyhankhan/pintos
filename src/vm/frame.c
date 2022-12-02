#include <stddef.h>
#include <stdio.h>

#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

static struct hash frame_table;
static struct lock lock_on_frame;

//compares keys in both hash_elems
bool less_compare_function(const struct hash_elem *first_hash_elem, const struct hash_elem *second_hash_elem, void *aux UNUSED) {
  const struct frame *first = hash_entry(first_hash_elem, struct frame, hash_elem);
  const struct frame *second = hash_entry(second_hash_elem, struct frame, hash_elem);
  return first->page < second->page;
}

//returns hash of hash_element's data
unsigned hashing_function(const struct hash_elem *hash_element, void *aux UNUSED) {
  const struct frame *frame = hash_entry(hash_element, struct frame, hash_elem);
  return hash_int((unsigned)frame->page);
}

//initialise frame table
void initialise_frame() {
  lock_init(&lock_on_frame);
  hash_init(&frame_table, hashing_function, less_compare_function, NULL);
}

static bool insert_page_into_frame(void *page_to_insert) {

  struct frame *frame = (struct frame *) malloc (sizeof (struct frame));
  if (frame == NULL) {
    return false;
  }  
  lock_acquire(&lock_on_frame);
  hash_insert(&frame_table, &frame->hash_elem);
  lock_release(&lock_on_frame); 
  frame->page = page_to_insert;
  frame->thread = thread_current ();

  return true;
}