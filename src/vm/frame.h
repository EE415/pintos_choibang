#include "lib/kernel/hash.h"

struct frame
{
  uint32_t *pte;              /* Page table entry pointer */
  void *addr;                 /* Physical address of the page */
  void *uaddr;                /* User virtual address */
  struct thread *thread       /* Thread using the frame */
  struct hash_elem hash_elem; /* Hash table element */
  bool evictable;             /* Flag for eviction */
}

