#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <stdio.h>
#include <list.h>

/*using supplement table, find the supplement page*/
struct sup_page *
find_sp(struct list *sp_table, void *upage)
{
	struct list_elem *e;
	struct sup_page *sp;
	lock_acquire(&thread_current()->sp_lock);
	if(!list_empty(sp_table))
	{
		e = list_front(sp_table); 
		while(e!=list_end(sp_table))
		{
			sp = list_entry(e, struct sup_page, elem);
			if(sp->upage == upage)
			{
				lock_release(&thread_current()->sp_lock);
				return sp;
			}
			e = list_next(e);
		}
	}
	lock_release(&thread_current()->sp_lock);
	return NULL;
}
/*add the supplement page when the thread's execution file.*/
bool
add_exefile_sp(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable, bool mmapFlag)
{
	struct sup_page *sp = (struct sup_page *)malloc(sizeof(struct sup_page));
	if(sp == NULL)
		return false;
	lock_acquire(&thread_current()->sp_lock);
	sp->fd = file->fd;
	sp->file = file;
	sp->ofs = ofs;
	sp->upage = upage;
	sp->read_bytes = read_bytes;
	sp->zero_bytes = zero_bytes;
	sp->writable = writable;
	sp->ss = NULL;
	sp->swapped = false;
	sp->mmapFlag = mmapFlag;
	list_push_back(&thread_current()->sp_table, &sp->elem);
	lock_release(&thread_current()->sp_lock);
	return true;
}

/*add the supplement page when swap*/
bool
add_new_sp(struct frame *f)
{
	struct thread *t = f->t;
	struct sup_page *sp = (struct sup_page *)malloc(sizeof(struct sup_page));
	if(sp == NULL)
		return false;
	lock_acquire(&thread_current()->sp_lock);
	sp->upage = f->page_addr;
	sp->ss = swap_out(f->frame_addr);
	sp->writable = f->writable;
	sp->swapped = true;
	if(sp->ss == NULL)
		return false;
	list_push_back(&t->sp_table, &sp->elem);
	pagedir_clear_page (t->pagedir, sp->upage);
	lock_release(&thread_current()->sp_lock);
	return true;
}

/*load the thread's exefile */
bool
load_exefile(struct sup_page *sp)
{
  void *kpage = frame_allocate_zeroflag(sp->upage, sp->mmapFlag, sp->writable, false);
  if (kpage == NULL)
    	return false;
    lock_acquire(&thread_current()->sp_lock);
    if (file_read_at (sp->file, kpage, sp->read_bytes, sp->ofs) != (int) sp->read_bytes)
    {
    	delete_single_frame(kpage);
    	lock_release(&thread_current()->sp_lock);
    	return false; 
    }
    lock_release(&thread_current()->sp_lock);
    memset (kpage + sp->read_bytes, 0, sp->zero_bytes);
    bool success = (pagedir_get_page (thread_current()->pagedir, sp->upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, sp->upage, kpage, sp->writable));
	if(!success)
	{
		delete_single_frame(kpage);
		return false;
	}
	if(sp->writable)
	  {
	    remove_sp(sp);
	  }
	
    return true;
}

/* load page to frame from swap disk */
bool
load_swap(struct sup_page *sp)
{
  void *kpage = frame_allocate(sp->upage, false,sp->writable);
  if(kpage == NULL)
    return false;
  bool success = (pagedir_get_page (thread_current()->pagedir, sp->upage) == NULL
		  && pagedir_set_page (thread_current()->pagedir, sp->upage, kpage, sp->writable));
  if(!success)
    {
      delete_single_frame(kpage);
      return false;
    }
  swap_in(kpage, sp->ss);
  remove_sp(sp);
  return true;
}

/* load the supplement page */
bool
load_sp(struct sup_page *sp)
{
	if(sp->swapped)
		return load_swap(sp);
	else
		return load_exefile(sp);
}
/*remove the supplement page in the supplement table and free it*/
void
remove_sp(struct sup_page *sp)
{
  lock_acquire(&thread_current()->sp_lock);
  list_remove(&sp->elem);
  free(sp);
  lock_release(&thread_current()->sp_lock);
}
/*when sys_exit, destroy the supplement page table */
void
destroy_spt(struct thread *t)
{
	struct list_elem *e;
	struct sup_page *sp;
	remove_thread_frame(t);
	lock_acquire(&thread_current()->sp_lock);
	while(!list_empty(&t->sp_table))
	{
		sp = list_entry(list_pop_front(&t->sp_table), struct sup_page, elem);
		if(sp->swapped)
			set_free_slot(sp->ss);
		free(sp);
	}
	lock_release(&thread_current()->sp_lock);
}
