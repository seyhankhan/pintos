#ifndef MMAP_H
#define MMAP_H
#include <hash.h>
#include "lib/kernel/list.h"
#include "filesys/file.h"
#include "lib/kernel/hash.h"
#include "lib/debug.h"

//mapping ID for memory mapped files
typedef int mapid_t;

void init_mmap(void);
void insert_mfile (mapid_t mapid, struct file *file, void* start_addr, void* end_addr);
struct memory_file *get_mfile(mapid_t mapid);
void delete_mfile(struct memory_file *mfile);


struct memory_file {
    mapid_t mapid;          /* Mapping ID - unique identifier of the Mfile*/     
    struct file *file;      /* Pointer to file that it's holding in memory */
    struct list_elem elem;  /* List elem to insert into list of memory mapped files*/  
    void *start_addr;       /* Start User Virtual Address*/
    void *end_addr;         /* End User virtual Address*/
};


#endif