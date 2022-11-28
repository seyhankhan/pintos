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

void release_page_from_frame(struct frame* frame) {
    if (!frame) {
        return;
    }
    frame->page = NULL;
}

void free_frame_table(struct page* page) {
    if (!page) {
        return;
    }
    int i;
    for (i = 0; i < curr_frames; i++) {
        lock_acquire(&lock_on_frame_table);
        if (frame_table[i] && frame_table[i]->page == page) {
            free(frame_table[i]);
            lock_release(&lock_on_frame_table);
            return;
        }
        lock_release(&lock_on_frame_table);
    }

}

void allocate_page_to_frame(struct page* page) {
    if (!page) {
        return;
    }
    int i;
    for (i = 0; i < curr_frames; i++) {
        lock_acquire(&lock_on_frame_table);
        if (frame_table[i]->page == NULL) {
            struct frame* frame = malloc(sizeof(struct frame*));
            frame->page = page;
            lock_release(&lock_on_frame_table);
            return frame_table;
        }
        lock_release(&lock_on_frame_table);
    }
    return;
    // this is where we implement page eviction process later
}

