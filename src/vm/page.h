#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "threads/thread.h"
#include <list.h>

enum page_type
{
  FILE_SYS,    /* page in file system */
  SWAP,        /* page in swap space */
  ZERO         /* all-zero page */
};

struct supplement_page
{
  enum page_type page_type;  /* page type to locate the page */
  void *vaddr;
  struct swap_slot *slot;
  struct list_elem elem; 
  
  struct file *file;
  uint32_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;

};


struct supplement_page * find_sp(struct list *, void *);
bool insert_sp(struct file *, uint32_t, uint8_t *, uint32_t, uint32_t, bool);
bool insert_swap_sp(struct frame *);
bool load_from_swap(struct supplement_page *);
bool load_exefile(struct supplement_page *);
bool load_sp(struct supplement_page *);
void delete_sp(struct supplement_page *);
void delete_all_sp(struct thread *);



#endif
