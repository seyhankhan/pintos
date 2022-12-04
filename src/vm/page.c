#include "page.h"


unsigned hash_func (const struct hash_elem *e, void *aux UNUSED) 
{
    struct spt_entry *spte = hash_entry(e, struct spt_entry, hash_elem);
    return hash_int((int) spte->page);
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  return hash_entry(a, struct spt_entry, hash_elem)->page 
        < hash_entry(b, struct spt_entry, hash_elem)->page;
}