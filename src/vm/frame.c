#include <stddef.h>

#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"

static size_t curr_frames;
static struct lock lock_on_frame_table;

//array to store frame table
static struct frame** frame_table;

void initialise_frame(int num_frames) {
    void* addr = 0;
    curr_frames = 0;
    frame_table = malloc(sizeof(struct frame*) * num_frames);

    //while possible keep allocating frames to frame table
    while (addr == palloc_get_page(PAL_USER)) {
        frame_table[num_frames] = malloc(sizeof(struct frame));
        frame_table[num_frames]->page = NULL;
        frame_table[num_frames]->addr = addr;
        lock_init(&frame_table[num_frames]->lock);
        curr_frames++;
    }
    lock_init(&lock_on_frame_table);

}