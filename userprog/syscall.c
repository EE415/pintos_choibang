#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static int syscall_write(int fd, const void *buffer, unsigned size);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/*[modified] project 2 : system call */

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t* sysptr = (uint32_t *)f->esp;
  switch(*sysptr)
    {
    case SYS_WRITE:
      f->eax = syscall_write(*(sysptr+1),*(sysptr+2),*(sysptr+3));
      break; 
    }
  f->eip = f->eax

  //printf ("system call!\n");
  thread_exit ();
}

static int
syscall_write(int fd, const void *buffer, unsigned size)
{
  
  if(fd == 1)
    {
      putbuf(buffer, size);
      return size;
    }
  if(fd == 0)
    return -1; 
  else 
    {
      printf("write syscall!");
      return -1;
    }
  
}

