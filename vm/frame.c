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
#include "threads/interrupt.h"
#include <stdio.h>
#include <list.h>

struct list ft_list;
struct list_elem *next_vict_elem;
struct lock frame_lock;

/*initialize the frame */
void
frame_init(void)
{
	list_init(&ft_list);
	next_vict_elem = NULL;
	lock_init(&frame_lock);
}

/*find the frame using the frame address */
struct frame *
find_frame(void *frame_addr)
{
	struct list_elem *e;
	struct frame *f;
	lock_acquire(&frame_lock);
	if(!list_empty(&ft_list))
	{
		e = list_front(&ft_list);
		while(e!=list_end(&ft_list))
		{
			f = list_entry(e, struct frame, elem);
			if(f->frame_addr == frame_addr)
			{
				lock_release(&frame_lock);
				return f;
			}
			e = list_next(e);
		}
	}

			lock_release(&frame_lock);
	return NULL;
}

/*clockwise algorithm*/
void
clockwise_victim(void)
{
	if(list_empty(&ft_list))
		next_vict_elem = NULL;
	else
	{
		if(list_next(next_vict_elem) == list_end(&ft_list))
		{
			if(list_front(&ft_list) == next_vict_elem)
				next_vict_elem = NULL;
			else
				next_vict_elem = list_front(&ft_list);
		}
		else
			next_vict_elem = list_next(next_vict_elem);
	}
}

/*select the victim using the second chance and clock algorithm*/
struct frame *
select_victim(void)
{
	if(!list_empty(&ft_list))
	  {
	    struct frame *f;
	    if(next_vict_elem == NULL)
	      next_vict_elem = list_front(&ft_list);
	    while(1)
	      {
		f = list_entry(next_vict_elem, struct frame, elem);
		clockwise_victim();
		if(pagedir_is_accessed(f->t->pagedir, f->page_addr))
		  pagedir_set_accessed(f->t->pagedir, f->page_addr,false); //second chance algorithm + clock algorithm
		else
		  break;
	      }
	    return f;
	  }
	return NULL;
}

/*allocate the frame of which zero is true */
void *
frame_allocate(void *upage, bool mmapFlag, bool writable)
{
  frame_allocate_zeroflag(upage, mmapFlag, writable, true);
}

/*allocate the frame if get page succeed, then add new frame. 
 If not replace frame.*/
void *
frame_allocate_zeroflag(void *upage, bool mmapFlag, bool writable, bool zero)
{
	void *kpage;
	if(zero)
		kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	else
		kpage = palloc_get_page (PAL_USER);
	if(kpage == NULL)
	{
	  kpage = replace_frame(upage, writable, zero);
	}
	else
	{
	  if(!add_new_frame(upage, kpage, mmapFlag, writable))
	    kpage = NULL;
	}
	return kpage;
}

/*add the new frame */
bool
add_new_frame(void *upage, void *kpage, bool mmapFlag, bool writable)
{
	ASSERT(find_frame(kpage) == NULL);
	lock_acquire(&frame_lock);
	struct frame *f = (struct frame *)malloc(sizeof(struct frame));
	if(f == NULL)
		return false;
	f->page_addr = upage;
	f->frame_addr = kpage;
	f->t = thread_current();
	f->writable = writable;
	f->mmapFlag = mmapFlag;
	
	if (mmapFlag)
	{
		struct sup_page *sp = find_sp(&thread_current()->sp_table, upage);
		f->ofs = sp->ofs;
		f->read_bytes = sp->read_bytes;
		f->zero_bytes = sp->zero_bytes;
		f->fd = sp->fd;
		f->file = sp->file;
	}
	list_push_back(&ft_list, &f->elem);
	lock_release(&frame_lock);
	return true;
}

/* return replaced kernel virtual address */
void *
replace_frame(void *upage, bool writable, bool zero)
{
	lock_acquire(&frame_lock);
	struct frame *vict = select_victim();
	if(!add_new_sp(vict))
	{
		lock_release(&frame_lock);	
		return NULL;
	}
	if(vict->mmapFlag)
		file_write_at (vict->file, vict->frame_addr, vict->read_bytes, vict->ofs);
	vict->page_addr = upage;
	vict->t = thread_current();
	vict->writable = writable;
	vict->mmapFlag = false;
	if(zero)
		memset(vict->frame_addr, 0, PGSIZE);
	lock_release(&frame_lock);
	return vict->frame_addr;
}

/*delete the single frame */
void
delete_single_frame(void *kpage)
{
	//printf("delete_single_frame\n");
	struct frame *f = find_frame(kpage);
	lock_acquire(&frame_lock);
	if(f != NULL)
	{
		list_remove(&f->elem);	
		free(f);
		palloc_free_page(kpage);
	}
	lock_release(&frame_lock);
}

/* unmap, delete, and free the frames. Used in UNMAP system call. */
void
unmap_frames(int fd)
{
	struct list_elem *e1;
	struct list_elem *e2;
	struct frame *f;
	lock_acquire(&frame_lock);
	if (!list_empty(&ft_list))
	{
		e1 = list_begin(&ft_list);
		while (e1 != list_end(&ft_list))
		{
			f = list_entry(e1, struct frame, elem);
			if ((f->fd == fd) && (f->mmapFlag))
			{
				file_write_at(f->file, f->frame_addr, f->read_bytes, f->ofs);
				e2 = list_next(e1);
				if (e1 == next_vict_elem)
					clockwise_victim();
				list_remove(e1);
				e1 = e2;
				pagedir_clear_page(f->t->pagedir, f->page_addr);
				palloc_free_page(f->frame_addr);
				free(f);
			} 
			else
				e1 = list_next(e1);
		}
	}
	lock_release(&frame_lock);
}

/*delete the all the thread frame. */
void *
remove_thread_frame(struct thread *t)
{
	struct list_elem *e1;
	struct list_elem *e2;
	struct frame *f;
	lock_acquire(&frame_lock);
	if(!list_empty(&ft_list))
	{
		e1 = list_front(&ft_list);
		while(e1!=list_end(&ft_list))
		{
			f = list_entry(e1, struct frame, elem);
			if(f->t->tid == t->tid)
			{
				if (f->mmapFlag)
				{
					file_write_at (f->file, f->frame_addr, f->read_bytes, f->ofs);
				}
				e2 = list_next(e1);
				if(e1 == next_vict_elem)
					clockwise_victim();
				list_remove(e1);
				e1 = e2;
				pagedir_clear_page(t->pagedir, f->page_addr);
				palloc_free_page(f->frame_addr);
				free(f);
			}
			else
				e1 = list_next(e1);
		}
	}
	lock_release(&frame_lock);
}

/*If page fault, in specific situation, grow the stack.*/
bool
stack_growth(void *fault_addr, struct intr_frame *f)
{
	void *kpage;
	struct thread *curr = thread_current();
	if ((fault_addr <= curr->stack_lim) && (fault_addr >= (f->esp - 32)) && (fault_addr >= (PHYS_BASE-(1<<23))))
	{
    	while (pg_round_down(fault_addr) < curr->stack_lim){
      		curr->stack_lim = (((uint8_t *) curr->stack_lim) - PGSIZE);
      		kpage = frame_allocate(curr->stack_lim, false,true);
      		if(kpage == NULL) 
      		{
      			delete_single_frame(kpage);
      			return false;
      		}
      		bool install = (pagedir_get_page (curr->pagedir, curr->stack_lim) == NULL
      			&& pagedir_set_page (curr->pagedir, curr->stack_lim, kpage, true));
      		if (!install)
      		{
        		delete_single_frame(kpage);
        		return false;
      		}
    	}
    	return true;
    }
    return false;
}
