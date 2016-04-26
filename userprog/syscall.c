#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

//static void syscall_handler (struct intr_frame *);
//static int syscall_write(int fd, const void *buffer, unsigned size);
//static int syscall_exit(int status);

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
    case SYS_HALT:
      power_off();
      break;

    case SYS_EXIT:
      f->eax =syscall_exit(*(sysptr+1));
      thread_exit();
      break;

    case SYS_WRITE:
      f->eax = syscall_write(*(sysptr+1),*(sysptr+2),*(sysptr+3));
      break;
    default :
      thread_exit();
      break;
    }

  //printf ("system call!\n");
  //thread_exit ();
}

static bool 
get_access(const void *mem)
{
  if(mem ==NULL || !is_user_vaddr(mem) || pagedir_get_page(thread_current()->pagedir, mem)==NULL)
    {
      thread_current() ->exit_value = -1; 
      thread_exit();
      return false;
    }
  return true;
}
static int
syscall_exit(int status)
{
  thread_current()->exit_value = status;
  return status;
}
  
static int
syscall_write(int fd, const void *buffer, unsigned size)
{
  if(get_access(buffer))
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
}

