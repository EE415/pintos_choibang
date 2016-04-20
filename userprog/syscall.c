#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

typedef int pid_t;
static void syscall_handler (struct intr_frame *);
static int syscall_exit(int status);
static pid_t syscall_exec(const char *cmd_line);
static int syscall_wait(pid_t pid);
static bool syscall_create(const char* file, unsigned initial_size);
static bool syscall_remove(const char *file);
static int syscall_open(const char *file);
static int syscall_filesize(int fd);
static int syscall_read(int fd, void *buffer, unsigned size);
static int syscall_write(int fd,const void *buffer, unsigned size);
static void syscall_seek(int fd, unsigned position);
static unsigned syscall_tell(int fd);
static void syscall_close(int fd);

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

    case SYS_CREATE:
      f->eax = syscall_create(*(sysptr+1),*(sysptr+2));
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
  if(mem ==NULL || !is_user_vaddr(mem))
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
syscall_exec(const char* cmd_line)
{
  return -1;
}

static int 
syscall_wait(pid_t pid)
{
  return 0;
}

static bool 
syscall_create(const char* file, unsigned initial_size)
{
  if(file == NULL)
    {
      syscall_exit(-1);
      thread_exit();
    }
  return filesys_create(file, initial_size);
}

static bool 
syscall_remove(const char* file)
{
  if(file == NULL)
    {
      syscall_exit(-1);
      thread_exit();
    }
  return filesys_remove(file);
}

static int 
syscall_open(const char* file)
{
  return 0;
}

static int 
syscall_filesize(int fd)
{
  return 0;
}

static int 
syscall_read(int fd, void *buffer, unsigned size)
{
  return 0;
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

static void 
syscall_seek(int fd, unsigned position)
{
  return ;
}

static unsigned 
syscall_tell(int fd)
{
  return 0;
}

static void 
syscall_close(int fd)
{
  return;
}


