#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <list.h>

struct swap_slot
{
  uint32_t location;
  struct list_elem elem;
};

void swap_init(void);
struct swap_slot *get_free_slot();
void set_free_slot(struct swap_slot *);
struct swap_slot * swap_out(void *);
void swap_in(void *, struct swap_slot *);

#endif
