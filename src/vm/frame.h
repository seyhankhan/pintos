#ifndef FRAME_H
#define FRAME_H

#include "threads/synch.h"
#include "vm/page.h"

struct frame {
    struct page* page;    // pointer to page (if any) occupying frame
    struct lock lock;     // allows synchronisation of frame processes 
    void* addr;           // frame's starting address
};



#endif