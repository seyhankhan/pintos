#ifndef FRAME_H
#define FRAME_H

struct frame {
    struct page *page;    // pointer to page (if any) occupying frame
    struct lock lock;     // allows synchronisation of frame processes 
    void* addr;           // frame's starting address
}; 

void initialise_frame(int num_frames);

#endif