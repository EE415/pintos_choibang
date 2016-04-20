#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void syscall_handler (struct intr_fram *);
int syscall_write(int, const void *, unsigned);
 
#endif /* userprog/syscall.h */
