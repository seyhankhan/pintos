#ifndef FRAME_H
#define FRAME_H

#include <hash.h>

#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/page.h"

struct frame {
    void *page;                 /* Actual page. */
    struct thread *thread;      /* Owner thread. */
    uint32_t *pte;              /* Page table entry of the frame's page. */
    void *uva;                  /* Address of the frame's page. */
    struct hash_elem hash_elem; /* Hash element for the hash frame table. */
};

bool less_compare_function(const struct hash_elem *first_hash_elem, const struct hash_elem *second_hash_elem, void *aux UNUSED);
unsigned hashing_function(const struct hash_elem *hash_element, void *aux UNUSED);
void initialise_frame(void);
void *obtain_free_frame(enum palloc_flags flags);
void free_frame_from_table(void* page);

#endif