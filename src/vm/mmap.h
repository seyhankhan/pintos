#ifndef MMAP_H
#define MMAP_H
#include <hash.h>
#include "lib/kernel/list.h"
#include "filesys/file.h"
#include "lib/kernel/hash.h"
#include "lib/debug.h"

//mapping ID for memory mapped files
typedef int mapid_t;

// void insert_memory_file(mapid_t mapid, int fid, void *start_addr, void *end_addr);
// bool delete_memory_file(mapid_t mapid);
void init_mmap(void);
// struct memory_file *find_memory_file(mapid_t mapid);
void insert_mfile (mapid_t mapid, struct file *file, struct list *file_pages);
struct memory_file *get_mfile(mapid_t mapid);
unsigned hash_mfile (const struct hash_elem *e, void *aux UNUSED);
bool hash_less_mfile (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
void delete_mfile(struct memory_file *mfile);

/* File page that will store the upage and kpage addresses of a single page of a mmapped file*/
struct file_page {
    struct list_elem elem;
    uint8_t *kpage;
    void *upage;
    
};

struct memory_file {
    mapid_t mapid;                   //mapping id for file
    struct file *file;               //file pointer
    struct list *file_pages;          // List of file pages that hold upage and kpage addresses
    struct hash_elem elem;    //list elemenet
    int num_of_pages;
};


#endif