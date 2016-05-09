#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/thread.h"
#include <list.h>
#include <stdio.h>

struct frame
{
  uint32_t *pte;              /* Page table entry pointer */
  void *addr;                 /* Physical address of the page */
  void *uaddr;                /* User virtual address */
  struct thread *thread;       /* Thread using the frame */
  struct list_elem elem;    /* Hash table element */
  bool evictable;           /* Flag for eviction */
  bool writable;

};

void frame_init(void);
void clockwise_victim(void);
struct frame *select_victim(void);
void * frame_get_page(void *, bool, bool);
struct frame *frame_search(void *);
void frame_free_page(void *);
void * delete_frame_all(struct thread *);
bool stack_growth(void *, struct intr_frame *);

#endif
