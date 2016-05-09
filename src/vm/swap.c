#include <stdio.h>
#include <list.h>
#include "devices/disk.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include <string.h>

struct disk *swap_disk;
struct list free_swap_list;
disk_sector_t slot_max;
disk_sector_t slot_count;
struct lock swap_lock;
#define PAGE_SECTOR_NUM PGSIZE/DISK_SECTOR_SIZE

void 
swap_init(void)
{
  list_init(&free_swap_list);
  swap_disk = disk_get(1,1);
  slot_max = disk_size(swap_disk);
  slot_count = 0;
  lock_init(&swap_lock);
}

struct swap_slot *
get_free_slot()
{
  struct swap_slot *slot;
  if(list_empty(&free_swap_list))
    {
      if(slot_count + PAGE_SECTOR_NUM > slot_max)
	return NULL;
      else
	{
	  slot = (struct swap_slot *)malloc(sizeof(struct swap_slot));
	  slot->location = slot_count;
	  slot_count += PAGE_SECTOR_NUM;
	}
    }
  else 
    slot = list_entry(list_pop_front(&free_swap_list), struct swap_slot, elem);
  return slot;
}

void 
set_free_slot(struct swap_slot *slot)
{
  list_push_back(&free_swap_list, &slot->elem);
}


struct swap_slot *
swap_out(void *buffer)
{
  int i;
  void *sector = malloc(DISK_SECTOR_SIZE);
  lock_acquire(&swap_lock);
  struct swap_slot *slot = get_free_slot();
  if(slot == NULL)
    {
      lock_release(&swap_lock);
      return NULL;
    }
  disk_sector_t location = slot->location;
  for(i = 0; i<PAGE_SECTOR_NUM; i++)
    {
      memcpy(sector, (uint8_t *)((uint32_t)buffer + DISK_SECTOR_SIZE*i), DISK_SECTOR_SIZE);
      disk_write(swap_disk, (disk_sector_t)(location+i), sector);
    }
  free(sector);
  lock_release(&swap_lock);
  return slot;
}

void 
swap_in(void *buffer, struct swap_slot *slot)
{
  int i;
  void *sector = malloc(DISK_SECTOR_SIZE);
  lock_acquire(&swap_lock);
  disk_sector_t location = slot->location;
  for(i = 0; i<PAGE_SECTOR_NUM;i++)
    {
      disk_read(swap_disk, (disk_sector_t)(location + i), sector);
      memcpy((uint8_t *)((uint32_t)buffer + DISK_SECTOR_SIZE*i), sector, DISK_SECTOR_SIZE);
    }
  set_free_slot(slot);
  free(sector);
  lock_release(&swap_lock);
}
