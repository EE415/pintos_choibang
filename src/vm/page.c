#include "vm/page.h"
#include "vm/frame.h"
#include <hash.h>
#include "threads/malloc.h"
#include "threads/pte.h"
#include <stdbool.h>
#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"


struct supplement_page *
find_sp(struct list *sp_table, void *vaddr)
{
  struct list_elem *e; 
  struct supplement_page *sp; 
  lock_acquire(&thread_current()->sp_lock);
  if(!list_empty(sp_table))
    {
      for(e = list_begin(sp_table); e != list_end(sp_table) ; e = list_next(e))
	{
	  sp = list_entry(e, struct supplement_page, elem);
	  if(sp->vaddr == vaddr)
	    {
	      lock_release(&thread_current()->sp_lock);
	      return sp;
	    }
	}
    }
  lock_release(&thread_current()->sp_lock);
  return NULL;
}

bool
insert_sp(struct file *f, uint32_t ofs, uint8_t *vaddr, uint32_t read_bytes, uint32_t zero_bytes,bool writable)
{
  struct supplement_page *sp = (struct supplement_page *)malloc(sizeof(struct supplement_page));
  if(sp == NULL)
    return false;
  lock_acquire(&thread_current()->sp_lock);
  sp->file = f;
  sp->ofs = ofs;
  sp->vaddr = vaddr;
  sp->read_bytes = read_bytes;
  sp->zero_bytes = zero_bytes;
  sp->slot = NULL;
  sp->page_type = FILE_SYS;
  sp->writable = writable;
  list_push_back(&thread_current()->sp_table, &sp->elem);
  lock_release(&thread_current()->sp_lock);
  return true;
}

bool 
insert_swap_sp(struct frame *f)
{
  struct thread *t = f->thread;
  struct supplement_page *sp = (struct supplement_page *)malloc(sizeof(struct supplement_page));
  if(sp==NULL)
    return false;
  lock_acquire(&thread_current()->sp_lock);
  sp->vaddr = f->uaddr;
  sp->slot = swap_out(f->addr);
  sp->page_type = SWAP;
  sp->writable = f->writable;
  if(sp->slot== NULL)
    return false;
  list_push_back(&t->sp_table, &sp->elem);
  pagedir_clear_page(t->pagedir, sp->vaddr);
  lock_release(&thread_current()->sp_lock);
  return true;
}

bool
load_from_swap(struct supplement_page *sp)
{
  void *kpage = frame_get_page(sp->vaddr, true, sp->writable);
  if(kpage == NULL)
    return false;
  bool success = pagedir_get_page(thread_current()->pagedir, sp->vaddr) == NULL &&
    pagedir_set_page(thread_current()->pagedir, sp->vaddr, kpage, sp->writable);
  if(!success)
    {
      frame_free_page(kpage);
      return false;
    }
  swap_in(kpage, sp->slot);
  delete_sp(sp);
  return true;
}


bool
load_exefile(struct supplement_page *sp)
{
  void *kpage = frame_get_page(sp->vaddr, false, sp->writable);
  if(kpage == NULL)
    return false;
  lock_acquire(&thread_current()->sp_lock);
  if(file_read_at(sp->file, kpage, sp->read_bytes, sp->ofs) != (int) sp->read_bytes)
    {
      frame_free_page(kpage);
      lock_release(&thread_current()->sp_lock);
      return false;
    }
  lock_release(&thread_current()->sp_lock);
  memset(kpage+sp->read_bytes, 0, sp->zero_bytes);
  bool success = pagedir_get_page(thread_current()->pagedir, sp->vaddr) == NULL && 
    pagedir_set_page(thread_current()->pagedir, sp->vaddr, kpage, sp->writable);
  if(!success)
    {
      frame_free_page(kpage);
      return false;
    }
  if(sp->writable)
    delete_sp(sp);
  return true;
}

bool
load_sp(struct supplement_page *sp)
{
  if(sp->page_type == SWAP)
    return load_from_swap(sp);
  else if(sp->page_type == FILE_SYS)
    return load_exefile(sp);
  else
    return false;
}

void 
delete_sp(struct supplement_page *sp)
{
  lock_acquire(&thread_current()->sp_lock);
  list_remove(&sp->elem);
  free(sp);
  lock_release(&thread_current()->sp_lock);
}

void 
delete_all_sp(struct thread *t)
{
  struct list_elem *e ;
  struct supplement_page *sp;
  delete_frame_all(t);
  lock_acquire(&thread_current()->sp_lock);
  while(!list_empty(&t->sp_table))
    {
      sp = list_entry(list_pop_front(&t->sp_table), struct supplement_page, elem);
      if(sp->page_type == SWAP)
	set_free_slot(sp->slot);
      free(sp);
    }
  lock_release(&thread_current()->sp_lock);
}


      
				  
