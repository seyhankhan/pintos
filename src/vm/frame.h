#ifndef FRAME_H
#define FRAME_H

#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/page.h"

struct frame {
    void *frame_page_address;                     // address of the frame's page
    struct hash_elem hash_elem;                   // hash entry for frame table
    struct thread *thread;                        // thread that owns frame 
    struct lock lock;                             // allows synchronisation of frame processes 
    void *page;                                   // pointer to page occupying frame
    uint32_t *page_table_entry;                   // frame's page's page table entry
};


bool less_compare_function(const struct hash_elem *first_hash_elem, const struct hash_elem *second_hash_elem, void *aux UNUSED);
unsigned hashing_function(const struct hash_elem *hash_element, void *aux UNUSED);
void initialise_frame(void);
void *obtain_free_frame(enum palloc_flags flags);
void free_frame_from_table(void* page);
bool map_user_vp_to_frame(void *page, uint32_t *page_table_entry, void *frame_page_address); 

#endif