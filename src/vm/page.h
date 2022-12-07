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
bool lazy_load_page(struct spt_entry *entry);
struct spt_entry *spt_find_addr(const void *addr);
struct spt_entry *spt_add_page(struct hash *spt, struct spt_entry *entry);
struct spt_entry *create_file_page(struct file *file, void *upage,  off_t ofs, 
                                   size_t read_bytes,size_t zero_bytes, bool writable);
struct spt_entry *create_zero_page(void *addr, bool writable);
bool load_page(struct spt_entry *page);

#endif 