#ifndef MMAP_H
#define MMAP_H
#include <hash.h>
#include <list.h>

//mapping ID for memory mapped files
typedef int mapid_t;

void insert_mfile (mapid_t mapid, int fid, void *start_addr, void *end_addr);

struct memory_file {
    mapid_t mapid;                   //mapping id for file
    int fid;                         //file descriptor
    struct hash_elem hash_elem;      //hash element 
    struct list_elem thread_elem;    //list elemenet
    void *start_addr;                //user vaddr of start of mapped file 
    void *end_addr;                  //user vaddr of end of mapped file
};


#endif