#ifndef PAGE_H
#define PAGE_H
#include <hash.h>
#include "filesys/off_t.h"

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

#endif 