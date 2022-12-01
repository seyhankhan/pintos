#ifndef FRAME_H
#define FRAME_H

#include <hash.h>
#include "threads/synch.h"
#include "vm/page.h"

struct frame {
    struct thread *thread;          // thread that owns frame 
    struct lock lock                // allows synchronisation of frame processes 
    void *page;                     // pointer to page occupying frame
    struct hash_elem hash_elem;     // hash entry for frame table
    void *addr;                     // address of the frame's page
};

#endif