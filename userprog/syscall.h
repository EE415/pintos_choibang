#include <list.h>
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct file_set
{
  struct file *f;
  int fd;
  struct list_elem elem;
};

void syscall_init (void);
void syscall_handler (struct intr_fram *);
int syscall_write(int, const void *, unsigned);
 
#endif /* userprog/syscall.h */
