#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>

void syscall_init (void);

void isUseraddr(int argnum, int pointer_index,struct intr_frame *);
struct file * fd2file(int fd);
#endif /* userprog/syscall.h */
