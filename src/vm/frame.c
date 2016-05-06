#include "vm/frame.h"
#include <stdio.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"

static struct hash frames;
static struct lock f_lock;

/* Returns true if frame a precedes frame b. */
bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct frame *a = hash_entry (a_, struct frame, hash_elem);
  const struct frame *b = hash_entry (b_, struct frame, hash_elem);
  return a->addr < b->addr;
}

/* Returns a hash value for frame f. */
unsigned frame_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const struct frame *f = hash_entry (f_, struct frame, hash_elem);
  return hash_int ((unsigned)frame->addr);
}

/* Initializes hash table of frames. */
void frame_init ()
{
  hash_init (&frames, frame_hash, frame_less, NULL);
  lock_init (f_lock);
}

/* Allocates a new page and adds the page to frame table. */
void *frame_get_page (void *uaddr, bool zero, void *kpage, struct thread *t)
{
  kpage = palloc_get_page (PAL_USER | (zero ? PAL_ZERO: 0));
  t = thread_current ();

  /* If memory is full, kernel panic for now. 
 * TODO: Implement eviction to get memory */
  if (kpage == NULL)
  {
    page_fault();
  }

  struct frame *f = (struct frame *) malloc (sizeof (struct frame));
  f->addr = kpage;
  f->uaddr = uaddr;
  f->thread = t;
  f->evictable = false;

  lock_acquire (&f_lock);
  hash_insert (&frames, &f->hash_elem);
  lock_release (&f_lock);

  return kpage;
}

/* Returns a frame with given physical address. Return NULL if no such frame. */
struct frame *frame_search (void *addr)
{
  struct frame *f;
  struct frame f_find;
  f_find.addr = addr;
  f = hash_entry (hash_find(&frames, &f_find.hash_elem), struct frame, hash_elem);
 
  if (f != NULL)
    return f;
  else
    return NULL;
}

void frame_free_page (void *addr)
{
  struct frame *f;
  f = frame_search (addr);

  if (f != NULL)
  {
    palloc_free_page (f->addr);
    hash_delete (&frames, &f->hash_elem);
    free (f);
    return;
  }
  else
    return;
}
