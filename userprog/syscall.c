#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/kernel/list.h"

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
static bool is_code_segment(void *addr);

#define start_addr 0x08048000
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
  if(f->esp < start_addr)
    {
      syscall_exit(-1);
      thread_exit();
    }

  switch(*sysptr)
    {
    case SYS_HALT:
      power_off();
      break;

    case SYS_EXIT:
      f->eax = syscall_exit(*(sysptr+1));
      thread_exit();
	
      break;
    
    case SYS_EXEC:
      f->eax = syscall_exec(*(sysptr+1));
      break;
      
    case SYS_WAIT:
      f->eax = syscall_wait(*(sysptr+1));
      break;
      
    case SYS_CREATE:
      f->eax = syscall_create(*(sysptr+1), *(sysptr+2));
      break;
      
    case SYS_REMOVE:
      f->eax = syscall_remove(*(sysptr+1));
      break;
      
    case SYS_OPEN:
      f->eax = syscall_open(*(sysptr+1));
      break;
      
    case SYS_FILESIZE:
      f->eax = syscall_filesize(*(sysptr+1));
      break;

    case SYS_READ:
      f->eax = syscall_read(*(sysptr+1), *(sysptr+2), *(sysptr+3));
      break;
      
    case SYS_WRITE:
      f->eax = syscall_write(*(sysptr+1), *(sysptr+2), *(sysptr+3));
      break;

    case SYS_SEEK:
      syscall_seek(*(sysptr+1), *(sysptr+2));
      break;

    case SYS_TELL:
      f->eax = syscall_tell(*(sysptr+1));
      break;

    case SYS_CLOSE:
      syscall_close(*(sysptr+1));
      break;
      
    default :
      thread_exit();
      break;
    }

}
static bool 
is_code_segment(void *addr)
{
  if(addr < start_addr || addr > 0x08068000)
    return false;
  return true;
}

static struct file_set *
find_file(struct list *list, int fd)
{
  struct list_elem *e;
  for(e = list_begin(list); e != list_end(list); e = list_next(e))
    {
      struct file_set *fs = list_entry(e, struct file_set, elem);
      if(fd == fs->fd)
	return fs;
    }
  syscall_exit(-1);
  thread_exit();
}

static int 
max_fd(struct list *list)
{
  int max = 0;
  struct list_elem *e;
  for (e = list_begin(list); e != list_end(list); e = list_next(e))
    {
      int fd = list_entry(e, struct file_set, elem)->fd;
      if( max <  fd)
	max = fd;
    }
  return max;
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
  /*TO DO IMPLEMENT*/
  return -1;
}

static int 
syscall_wait(pid_t pid)
{
  /*TO DO IMPLEMENT*/
  return 0;
}

static bool 
syscall_create(const char* file, unsigned initial_size)
{
  if(file == NULL || !is_code_segment(file))
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
  if(file == NULL || !is_code_segment(file)) //code segment has 128M.
    {
      syscall_exit(-1);
      thread_exit();
    }

  /*Exception : none or missing file */
  if(strcmp(file, "") == 0 || strcmp(file, "no-such-file") == 0)
    return -1;
  
  /*Make struct file_set(struct file, int fd, elem) and setting file and fd */
  struct file_set *fs = (struct file_set *)malloc(sizeof(struct file_set));
  fs->f = filesys_open(file);
  if(list_empty(&thread_current()->file_list))
    fs->fd = 2;
  else 
    fs->fd = max_fd(&thread_current()->file_list) +1;
  list_push_back(&thread_current()->file_list, &fs->elem);
  return fs->fd;
  
}

static int 
syscall_filesize(int fd)
{
  struct file_set *fs = find_file(&thread_current()->file_list, fd);
  return file_length(fs->f);
}

static int 
syscall_read(int fd, void *buffer, unsigned size)
{
  if(!is_user_vaddr(buffer))
    {
      syscall_exit(-1);
      thread_exit();
    }
  if(fd == 0)
    return input_getc();

  struct file_set *fs = find_file(&thread_current()->file_list, fd);
  return file_read(fs->f, buffer, size);
}

static int
syscall_write(int fd, const void *buffer, unsigned size)
{
  
  if(size == 0)
    return 0;
  
  if(fd == 1)
    {
      putbuf(buffer, size);
      return size;
    }
  
  if(!is_code_segment(buffer))
    {
      syscall_exit(-1);
      thread_exit();
    }
  
  struct file_set *fs = find_file(&thread_current()->file_list, fd);
  return file_write(fs->f, buffer, size);
  
}

static void 
syscall_seek(int fd, unsigned position)
{
  /*TO DO IMPLEMENT*/
  return ;
}

static unsigned 
syscall_tell(int fd)
{
  /*TO DO IMPLEMENT*/
  return 0;
}

static void 
syscall_close(int fd)
{
  struct file_set *fs = find_file(&thread_current()->file_list, fd);
  list_remove(&fs->elem);
  file_close(fs->f);
}


