#include <stddef.h>

#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"



struct frame 
  {
    struct thread *thread;          // thread that owns frame 
    struct lock lock                // allows synchronisation of frame processes 
    void *page;                     // pointer to page occupying frame
    struct hash_elem hash_elem;     // hash element for the hash frame table
    void *addr;                     // address of the frame's page
  };