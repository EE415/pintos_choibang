#include "vm/frame.h"
#include <stdio.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"


struct list frames;
struct lock f_lock;
struct list_elem *victim_elem;

/* Initializes list of frames. */
void frame_init ()
{
  list_init (&frames);
  lock_init (&f_lock);
  victim_elem = NULL;
}

/* Clockwise Algorithm */
void clockwise_victim (void)
{
  if (list_empty(&frames) || (list_size(&frames) == 1))
    victim_elem = NULL;
  else
  {
    if (list_next(victim_elem) == list_end(&frames))
      victim_elem = list_front (&frames);
    else
      victim_elem = list_next (victim_elem);
  }
}

/* Choose victim using clockwise algorithm */
struct frame *select_victim (void)
{
  struct frame *victim;
  if (victim_elem == NULL)
    victim_elem = list_front (&frames);
  while (1)
  {
    victim = list_entry (victim_elem, struct frame, elem);
    clockwise_victim();
    if (pagedir_is_accessed(victim->thread->pagedir, victim->addr))
      pagedir_set_accessed(victim->thread->pagedir, victim->addr, false);
    else
      break;
  }
  return victim;
}

/* Allocates a new page and adds the page to frame table. */
void *frame_get_page (void *uaddr, bool zero, bool writable)
{
  void *kpage;
  kpage = palloc_get_page (PAL_USER | (zero ? PAL_ZERO: 0));
  struct thread *t = thread_current ();

  /* If memory is full, kernel panic for now. 
 * TODO: Implement eviction to get memory */
  
  if (kpage == NULL)
  {
    lock_acquire(&f_lock);
    struct frame *v = select_victim();
    if (!insert_sp(v))
    {
      lock_release(&f_lock);
      return NULL;
    }
    v->uaddr = uaddr;
    v->thread = thread_current ();
    v->writable = writable;
    if (zero)
      memset(v->addr, 0, PGSIZE);
    lock_release(&f_lock);
    kpage = v->addr;
  }
  else
  {
    /* Allocate frame */
    lock_acquire(&f_lock);
    struct frame *f = (struct frame *) malloc (sizeof (struct frame));
    if (f == NULL)
      kpage = NULL;
    else
    {
      f->addr = kpage;
      f->uaddr = uaddr;
      f->thread = t;
      f->evictable = false;
      f->writable = writable;

      list_push_back (&frames, &f->elem);
      lock_release (&f_lock);    
    }
  }
  return kpage;
}

/* Returns a frame with given physical address. Return NULL if no such frame. */
struct frame *frame_search (void *addr)
{
  struct frame *f;
  struct list_elem *e;
  lock_acquire(&f_lock);
  if (!list_empty(&frames))
  {
    for (e = list_begin(&frames); e != list_end(&frames); e = list_next(e))
    {
      f = list_entry(e, struct frame, elem);
      if (f->addr == addr)
      {
        lock_release(&f_lock);
        return f;
      }
    }
  }
  lock_release(&f_lock);
  return NULL;
}

void frame_free_page (void *addr)
{
  struct frame *f;
  lock_acquire(&f_lock);
  f = frame_search (addr);

  if (f != NULL)
  {
    palloc_free_page (f->addr);
    list_remove (&f->elem);
    free (f);
    lock_release(&f_lock);
    return;
  }
  else
  {
    lock_release(&f_lock);
    return;
  }
}

void 
delete_frame_all(struct thread *t)
{
  struct list_elem *cur;
  struct list_elem *next;
  struct frame *f;
  lock_acquire(&f_lock);
  if(!list_empty(&frames))
    {
      /*for(cur = list_begin(&frames) ; cur != list_end(&frames) ; cur = list_next(cur))
	{
	  f = list_entry(cur, struct frame, elem);
	  if(f->thread->tid == t->tid)
	    {
	      //next = list_next(cur);
	      if(cur == victim_elem)
		clockwise_victim();
	      list_remove(cur);
	      //if (next != list_end(&frames))
              //  cur = next;
	      pagedir_clear_page(t->pagedir, f->uaddr);
	      palloc_free_page(f->addr);
	      free(f);
	    }
	    }*/
      cur = list_front(&frames);
      while(cur != list_end(&frames))
	{
          f = list_entry(cur, struct frame, elem);
	  next = list_next(cur);
	  if(f->thread->tid == t->tid)
	    {
	      if(cur == victim_elem)
		clockwise_victim();
	      list_remove(cur);
	      pagedir_clear_page(t->pagedir, f->uaddr);
	      palloc_free_page(f->addr);
	      free(f);
	    }
	  cur = next;
	}
    }
  lock_release(&f_lock);
}


bool 
stack_growth(void *fault_addr, struct intr_frame *f)
{
  void *kpage ;
  struct thread *curr = thread_current();
  if ((fault_addr <= curr->bound_stack) && (fault_addr >= (f->esp - 32)) && (fault_addr >= (PHYS_BASE - (1<<23))))
    {
      while(pg_round_down(fault_addr) < curr->bound_stack)
	{
	  curr->bound_stack = ((uint8_t *) curr->bound_stack) - PGSIZE;
	  kpage = frame_get_page(curr->bound_stack, true, true);
	  if(kpage == NULL)
	    {
	      frame_free_page(kpage);
	      return false;
	    }
	  bool success = (pagedir_get_page(curr->pagedir, curr->bound_stack) == NULL) && 
	    (pagedir_set_page(curr->pagedir, curr->bound_stack,kpage,true));
	  if(!success)
	    {
	      frame_free_page(kpage);
	      return false;
	    }
	}
      return true;
    }
  return false;
}

