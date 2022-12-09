#ifndef FRAME_H
#define FRAME_H

#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "devices/swap.h"


struct frame {
    void *kpage;                                  // address of the frame's page
    void *upage;                                  // User address that maps to  frame
    struct list_elem list_elem;                   // List elem for page eviction algorithm
    struct hash_elem hash_elem;                   // Hash entry for frame table
    bool is_pinned;                               // boolean check whether frame is pinned or not
    // struct list pages;
    bool reference_bit;
};


bool less_compare_function(const struct hash_elem *first_hash_elem, const struct hash_elem *second_hash_elem, void *aux UNUSED);
unsigned hashing_function(const struct hash_elem *hash_element, void *aux UNUSED);
void initialise_frame(void);
void *get_free_frame(enum palloc_flags flags);
void free_frame_from_table(void* page);
struct frame *get_frame_from_table(void *page_to_retrieve);


#endif