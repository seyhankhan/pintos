#ifndef PAGE_H
#define PAGE_H

struct page {
    struct frame* frame;     // frame associated with this page
    void* addr;              // page's virtual address
};

#endif 