#ifndef FRAME_H
#define FRAME_H

#include <hash.h>
#include "threads/synch.h"
#include "vm/page.h"

struct frame {
    struct page* page;          // pointer to page (if any) occupying frame
    struct lock lock;           // allows synchronisation of frame processes 
    void* addr;                 // frame page address
    struct hash_elem hash_elem; // hash entry for frame table

};



#endif