#ifndef PAGE_H
#define PAGE_H
#include <hash.h>
#include "filesys/off_t.h"
#include "lib/kernel/hash.h"
#include "debug.h"

struct page {
    struct frame* frame;     // frame associated with this page
    void* addr;              // page's virtual address
};

struct spt_entry {
    struct hash_elem hash_elem;

    struct file *file;
    off_t ofs;
    uint8_t *upage;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
    uint32_t page;
};

unsigned hash_func(const struct hash_elem *e, void *aux UNUSED);
bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

#endif 