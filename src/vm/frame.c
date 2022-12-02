#include <stddef.h>

#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "lib/kernel/hash.h"

// OLD
static size_t curr_frames;
static struct lock lock_on_frame_table;
static struct frame **frame_table;

// hash table to store frame entries
static struct hash frame_hash_table;

unsigned frame_hash(const struct hash_elem *f_, void *aux)
{
	const struct frame *f = hash_entry(f_, struct frame, hash_elem);
	return hash_int(&f->addr);
}

bool frame_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
	const struct frame *a = hash_entry(a_, struct frame, hash_elem);
	const struct frame *b = hash_entry(b_, struct frame, hash_elem);
	return a->addr < b->addr;
}

void initialise_frame(int num_frames)
{
	bool success = hash_init(&frame_hash_table, frame_hash, frame_less, NULL);
	// TODO: if (!success)
	lock_init(&lock_on_frame_table);
}

void release_page_from_frame(struct frame *frame)
{
	hash_delete(&frame_hash_table, &frame->hash_elem);
}

void free_frame(struct hash_elem *element, void *aux UNUSED)
{
	struct frame *frame = hash_entry(element, struct frame, hash_elem);
	free(frame->page);
	free(frame);
}

void free_frame_table(struct page *page)
{
	hash_destroy(&frame_hash_table, &free_frame);
}

struct frame *
allocate_page_to_frame(struct page *page)
{
	struct page *p = palloc_get_page(PAL_USER);
	struct frame *frame = malloc(sizeof(struct frame *));
	frame->page = page;
	frame->addr = &p;
	hash_insert(&frame_hash_table, frame);
}

/* -------------------------------------------------------------------------- */

void OLD_initialise_frame(int num_frames)
{
	void *addr;
	curr_frames = 0;
	frame_table = malloc(sizeof(struct frame *) * num_frames);

	// while possible keep allocating frames to frame table
	while (addr == palloc_get_page(PAL_USER))
	{
		frame_table[num_frames] = malloc(sizeof(struct frame));
		frame_table[num_frames]->page = NULL;
		frame_table[num_frames]->addr = addr;
		lock_init(&frame_table[num_frames]->lock);
		curr_frames++;
	}
	lock_init(&lock_on_frame_table);
}

void OLD_release_page_from_frame(struct frame *frame)
{
	if (!frame)
		return;

	frame->page = NULL;
}

void OLD_free_frame_table(struct page *page)
{
	if (!page)
		return;

	unsigned i;
	for (i = 0; i < curr_frames; i++)
	{
		lock_acquire(&lock_on_frame_table);
		if (frame_table[i] && frame_table[i]->page == page)
		{
			free(frame_table[i]);
			lock_release(&lock_on_frame_table);
			return;
		}
		lock_release(&lock_on_frame_table);
	}
}

struct frame *OLD_allocate_page_to_frame(struct page *page)
{
	if (!page)
		return NULL;

	unsigned i;
	for (i = 0; i < curr_frames; i++)
	{
		// lock_acquire(&lock_on_frame_tabllock_acquire(&lock_on_frame_table));
		if (frame_table[i]->page == NULL)
		{
			struct frame *frame = malloc(sizeof(struct frame *));
			frame->page = page;
			lock_release(&lock_on_frame_table);
			return frame_table[i];
		}
		// lock_release(&lock_on_frame_table);
	}

	// just so returns correct type. NEED TO IMPLEMENT PAGE EVICTIO PROCESS HERE LATER
	return frame_table[curr_frames];
}
