#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


#include <stdio.h>


void syscall_init (void);
void exit(int code);
bool is_valid_uaddr(const void *uaddr);

#endif /* userprog/syscall.h */
