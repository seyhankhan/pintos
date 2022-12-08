#ifndef PAGE_H
#define PAGE_H
#include <hash.h>
#include "filesys/off_t.h"
#include "lib/kernel/hash.h"
#include "lib/debug.h"

struct page {
    struct frame* frame;        // frame associated with this page
    void* addr;                 // page's virtual address
    bool can_write;             // whether we can write to this page or not
    struct list_elem list_elem;  // list element for page
    struct spt_entry* data;     // holds file data
    uint32_t pagedir;
    bool is_loaded;
    void* kpage;
};


struct spt_entry {
    struct hash_elem hash_elem;

    struct file *file;
    off_t ofs;
    uint8_t *upage;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
};

unsigned hash_func(const struct hash_elem *e, void *aux UNUSED);
bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
bool load_page_from_spt(struct spt_entry *entry);
struct spt_entry *spt_find_addr(const void *addr);
struct spt_entry *spt_add_page(struct hash *spt, struct spt_entry *entry);
bool spt_delete_page (struct hash *spt, void *page);
struct spt_entry *create_file_page(struct file *file, void *upage,  off_t ofs, 
                                   size_t read_bytes,size_t zero_bytes, bool writable);
struct spt_entry *create_zero_page(void *addr, bool writable);

#endif 