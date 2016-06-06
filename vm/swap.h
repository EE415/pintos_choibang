#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "threads/thread.h"
#include <list.h>

struct swap_slot
{
	uint32_t sec_no;			/* sector number of free swap slot in disk */
	struct list_elem elem;
};

void swap_init(void);
struct swap_slot *get_free_slot();
void set_free_slot(struct swap_slot *ss);
struct swap_slot *swap_out(void *buffer);
void swap_in(void *buffer, struct swap_slot *ss);

#endif /* vm/swap.h */