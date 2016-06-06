#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/thread.h"
#include <list.h>

struct frame *find_frame(void *frame_addr);
struct frame *select_victim(void);
/* Struct frame. Elements of frame table by using list structure. */


struct frame{
  struct thread* t;			/* thread pointer that owns frame */
  void *frame_addr;			/* address of physical frame */
  void *page_addr;			/* address of page mapped to this frame */
  bool writable;			/* frame writable or not */
  
  bool mmapFlag;			/* frame memory-mapped or not*/
  struct list_elem elem;		

  int fd;				/* file descriptor, also used as mapID of mmaped files */
  struct file *file;			 
  uint32_t ofs;				/* offset for mmaped frames */
  uint32_t read_bytes;			/* read_bytes for mmaped frames */
  uint32_t zero_bytes;			/* zero_bytes for mmaped frames */
};

void frame_init(void);
struct frame *find_frame (void *);
void clockwise_victim (void);
struct frame *select_victim (void);
void *frame_allocate(void *upage,bool mmapFlag, bool writable);
void *frame_allocate_zeroflag(void *upage, bool mmapFlag, bool writable, bool zero);
bool add_new_frame(void *upage, void *kpage, bool mmapFlag, bool writable);
void *replace_frame(void *upage, bool writable, bool zero);
void unmap_frames (int);

void delete_single_frame(void *kpage);
void *remove_thread_frame(struct thread *t);
bool stack_growth (void *, struct intr_frame *);

#endif /* vm/frame.h */
