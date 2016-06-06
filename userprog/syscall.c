#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "threads/init.h"
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "lib/user/syscall.h"

static void syscall_handler (struct intr_frame *);
struct thread * curr;
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  struct lock sys_lock;
  lock_init(&sys_lock);
  curr = thread_current();

  /* Variables uesd by system calls of several kinds*/
	int fd;
	char *buffer;
	int length;
	struct list_elem *find;
	struct file *file;
	unsigned position;

  /* Check the address of esp */
	isUseraddr(0,0,f);
  if(!pagedir_get_page(curr->pagedir, f->esp))
	  sys_exit(-1);

  int sys_num = *(int *)(f->esp);
  switch(sys_num)
  {
	// pid_t exec (const char *file)
	case SYS_EXEC:
	  isUseraddr(1,1,f);
		if(*(char **)(f->esp + 4) == NULL){
			f->eax = TID_ERROR;
			break;
		}

 		f->eax = process_execute( *(char **)(f->esp + 4));
		break;
	
	//bool create (const char *file, unsigned initial_size)
	case SYS_CREATE:
	  isUseraddr(2,1,f);
		
  		lock_acquire(&sys_lock);
		f->eax = filesys_create( *(char **)(f->esp+4), *(off_t *)(f->esp+8));
  		lock_release(&sys_lock);
		break;

	// void exit (int status)
	case SYS_EXIT:
	  isUseraddr(1,0,f);
		sys_exit(*(int *)(f->esp+4));
		break;

	//int read (int fd, void *buffer, unsigned size)
	case SYS_READ:
	  isUseraddr(3,2,f);
  		lock_acquire(&sys_lock);
		fd = *(int*)(f->esp+4);
		buffer = *(char**)(f->esp+8);	
		length = *(int*)(f->esp+12);
		void *upage = pg_round_down(buffer);
		void *kpage = pagedir_get_page(thread_current()->pagedir, upage);
		struct frame *fr = find_frame(kpage);
		if (fr == NULL)
		{
		  sys_exit(-1);
		  //thread_exit();
		  
		}
		if (!fr->writable)
		{
		  sys_exit(-1);
		}	
		/* stdin case*/
		if(fd == 0){
			int i = 0;

			uint8_t returnc;
			/* read untill null character appears or length is done*/
			while(i < length){
			returnc = input_getc();
			if(returnc=='\n')
			break;
			buffer[i++]=returnc;
			}
			f->eax = i;

		}
		/* stdout in case*/
		else if(fd ==1)
		f->eax = -1;
		/* normal case read from file*/
		else{
			file=NULL;
			bool suc=false;
			for (find = list_begin(&curr->fd_list);
			find != list_end(&curr->fd_list);
			find = list_next(find))
			{
				file = list_entry(find, struct file, elem);
				if(file->fd == fd)
				{
					suc=true;
					break;
				}
				
			}
			if(suc==false){
				lock_release(&sys_lock);
				f->eax = -1;
				break;
			}
			
			f->eax = file_read(file, buffer, length);
		}
		lock_release(&sys_lock);
		break;

	//int filesize (int fd)
	case SYS_FILESIZE:
	  isUseraddr(1,0,f);
  		lock_acquire(&sys_lock);
		fd = *(int *)(f->esp + 4);
		
		/* find file from fd_list*/	
		for(find = list_begin(&curr->fd_list);
		find != list_end(&curr->fd_list);
		find = list_next(find))
		{
			file = list_entry(find, struct file, elem);
			if(file->fd == fd)
				break;
		}
		if(file == NULL){
			lock_release(&sys_lock);
			f->eax = -1;
			break;
		}
		
		f->eax = file_length(file);
	

		lock_release(&sys_lock);
		break;

    // int wait (tid_t tid)
	case SYS_WAIT:
	  isUseraddr(1,0,f);
		tid_t pid = *(tid_t*)(f->esp+4);
		f->eax = process_wait(pid);
		break;

    // int open (const char * file)
  	case SYS_OPEN:
	  
	  isUseraddr(1,1,f);
  		lock_acquire(&sys_lock);
		
  		char * name = *(char **)(f->esp+4);
		file = filesys_open(name);
		/* null pointer*/
		if(file == NULL)
			f->eax = -1;
		else
		{
			/* assgin fd_num*/
			f->eax = curr->fd_num;
			file->fd= curr->fd_num;
			curr->fd_num++;

			/* push fd information to thread*/
			list_push_back(&curr->fd_list, &file->elem);
		}
		lock_release(&sys_lock);
		break;
		
  
    // int write (int fd, const void *buffer, unsigned length);
	case SYS_WRITE:
	  isUseraddr(3,2,f);
		lock_acquire(&sys_lock);

		fd = *(int*)(f->esp+4);
		buffer = *(char**)(f->esp+8);	
		length = *(int*)(f->esp+12);

		/* delete mmaped page to allow overwrite */	
		if (!list_empty(&curr->sp_table))
		  {
		    find = list_begin(&curr->sp_table);
		    while(find != list_end(&curr->sp_table))
		      {
			struct sup_page *sp_rm = list_entry(find, struct sup_page, elem);
			struct list_elem *next = list_tail(&curr->sp_table);
			if(find != list_prev(list_end(&curr->sp_table)))
			  next = list_next(find);
			if (sp_rm->fd == fd)
			  {
			    remove_sp(sp_rm);
			  }
			find = next;
		      }
		  }
		/* delete mapped frame to allow overwrite */
		unmap_frames (fd);
		
		/*stdin case*/
		if(fd == 0)
			f->eax=-1;
		
		/*stdout case*/
		else if(fd==1)
		{
			putbuf(buffer, length);
			f->eax=length;
		}
		/*normal case write from file*/
		else
		{
			file = fd2file(fd);

			if(!file)
				f->eax = -1;
			else{
				f->eax = file_write(file, buffer, length);		
			}
		}
		lock_release(&sys_lock);
		break;

	//void seek (int fd, unsigned position)
	case SYS_SEEK:
	  isUseraddr(2,0,f);
		fd = *(int*)(f->esp+4);
		position = *(unsigned*)(f->esp+8);

		file = fd2file(fd);
		file_seek(file, (off_t)position);
		
		break;

	// void close (int fd)
	case SYS_CLOSE:
	  isUseraddr(1,0,f);
		fd = *(int*)(f->esp+4);

		file = fd2file(fd);

		/* Remove the elem of the file in the fd_list of the thread */
		enum intr_level old_level = intr_disable ();
		list_remove(&file->elem);
		intr_set_level (old_level);

		/* Close the file */
		file_close(file);
		break;

	case SYS_TELL:
	  isUseraddr(1,0,f);
		fd = *(int*)(f->esp+4);

		file = fd2file(fd);

		f->eax = file_tell(file);
		break;

	case SYS_HALT:
		power_off();
		break;
		
        case SYS_MUNMAP:
	  lock_acquire(&sys_lock);
	   
	  mapid_t mapID =*(mapid_t *) (f->esp+4);

	  /* Remove all mmaped pages corresponding to mapID. */
	  if (!list_empty(&curr->sp_table))
	  {
	    find = list_begin(&curr->sp_table);
	    while(find != list_end(&curr->sp_table))
		  {
			struct sup_page *sp_rm = list_entry(find, struct sup_page, elem);
			struct list_elem *next = list_tail(&curr->sp_table);
			if(find != list_prev(list_end(&curr->sp_table)))
			  next = list_next(find);
			if (sp_rm->fd == mapID)
			    load_sp(sp_rm);
			find = next;
	  	  }
	  }
	  /* Remove all mmaped frames corresponding to mapID. */	  
	  unmap_frames (mapID);
	  lock_release(&sys_lock);
	  break;

        case SYS_MMAP:
	  lock_acquire(&sys_lock);
	  fd = *(int *)(f->esp+4);
	  uint32_t addr = *(uint32_t *)(f->esp+8);

	  /* Check invalid address. */
	  if(addr < 0x10000000 || addr > 0xb0000000)
	    {
	      lock_release(&sys_lock);
	      f->eax = -1;
	      break;
	    }

	  struct file *found_file;

	  /* Find file from current thread's file list. */
	  for(find = list_begin(&curr->fd_list);
	      find != list_end(&curr->fd_list);
	      find = list_next(find))
	    {
	      found_file = list_entry(find, struct file, elem);
	      if(found_file->fd == fd)
		break;
	    }

	  file = file_reopen(found_file);
	  file->fd = found_file->fd;

	  if(file == NULL)
	    {
	      lock_release(&sys_lock);
	      f->eax = -1;
	      break;
	    }

	  if(file_length(file) <= 0)
	    {
	      lock_release(&sys_lock);
	      f->eax = -1;
	      break;
	    }

	  /* Check if stdin or stdout. */
	  if(fd == 0 || fd == 1)
	    {
	      lock_release(&sys_lock);
	      f->eax = -1;
	      break;
	    }

	  if(addr == NULL || addr == 0 || (pg_ofs(addr) != 0))
	    {
	      lock_release(&sys_lock);
	      f->eax = -1;
	      break;
	    }

	  struct sup_page *sp = find_sp(&curr->sp_table, addr);
	  if(sp != NULL)
	    {
	      lock_release(&sys_lock);
	      f->eax = -1;
	      break;
	    }

	  uint32_t read_bytes = (file_length(file) < PGSIZE) ? file_length(file) : PGSIZE ;
	 
	  /* Add pages corresponding to file to be memory-mapped. 
 * 	     First, add one page and see if the file size is bigger than the page size.
 * 	     If it is bigger, then keep adding pages until it reaches end of file. */ 
	  add_exefile_sp(file, 0, addr, read_bytes, PGSIZE - read_bytes, true, true);
	  int cnt = 0;
	  while(PGSIZE == read_bytes)
	    {
	      cnt += 1;
	      sp = find_sp(&curr->sp_table, addr + cnt*PGSIZE);
	      if(sp!=NULL)
		{
		  /* Remove already-mapped pages if overlap occurs. */
		  struct list_elem *e;
		  for (e = list_begin(&curr->sp_table);
			e != list_end(&curr->sp_table);
			e = list_next(e))
		    {
		      struct sup_page *sp_rm = list_entry(e, struct sup_page, elem);
		      if (sp_rm->fd == fd)
		        remove_sp(sp_rm);
		    }
		  lock_release(&sys_lock);
		  f->eax = -1;
		  goto error;
		}
	      read_bytes = (file_length(file) < PGSIZE * (cnt+1)) ? file_length(file) - cnt* PGSIZE : PGSIZE;
	      add_exefile_sp(file, cnt*PGSIZE, addr+cnt*PGSIZE, read_bytes, PGSIZE-read_bytes, true, true);
	    } 
	  f->eax = file->fd;
	  lock_release(&sys_lock);
	  
  error :
	  break;
		
  case SYS_REMOVE :
    lock_acquire(&sys_lock);
  
    char * file_name  = *(char **)(f->esp+4);
    if(file_name == NULL)
      {
	lock_release(&sys_lock);
	f->eax = -1;
	break;
      }
    if(!filesys_remove(file_name))
      {
	lock_release(&sys_lock);
	f->eax = -1;
	break;
      }

    lock_release(&sys_lock);
    break;

  }
}


//check that the address is user address and check it is valid address
void
isUseraddr(int argnum, int pointer_index, struct intr_frame *f)
{
	//check it is user address 
	if((uint32_t)f->esp + 4 + argnum*4 > (uint32_t) PHYS_BASE)
	  {
	    sys_exit(-1);
	  }

	//check the pointer address is valid
	if(pointer_index > 0)
	{
		char **temp= (char**)(f->esp+pointer_index*4);

		if((uint32_t)*temp >= (uint32_t) PHYS_BASE)
		  {
		    sys_exit(-1);
		  }

		// char pointer validation check
		if(!pagedir_get_page(curr->pagedir, *temp))
		  {
		    struct supplement_page *sp = find_sp(&thread_current()->sp_table, pg_round_down(*temp));
		    if(sp!=NULL)
		      {
			load_sp(sp);
			return;
		      }
		    if(!stack_growth(*temp,f))
		      sys_exit(-1);
		  }
	}

	
}
//change fd to file pointer
struct file *
fd2file(int fd)
{
	struct list_elem *find;
	for (find = list_begin(&thread_current()->fd_list);
		find != list_end(&thread_current()->fd_list);
		find = list_next(find))
	{
		struct file * f = list_entry(find, struct file, elem);
		
		if(fd== f->fd)
		return f;
	}

	//can not find fd
	return 0;
}
