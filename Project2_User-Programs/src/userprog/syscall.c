#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "pagedir.h"

static void syscall_handler (struct intr_frame *);
static int write(int fd,void * buffer, size_t size);
//static void exit(int status);

static bool is_valid_addr(const uint8_t *uaddr) {
  if (is_user_vaddr(uaddr)) {
    return (pagedir_get_page(thread_current()->pagedir, uaddr) == NULL);
  }
  return false;
}


void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


// static void exit(int status) {
//   thread_exit();
// }


static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);


  if (args[0] > PHYS_BASE){
    thread_exit();
  }


  //printf("System call number: %d\n", args[0]);

  switch(args[0]){
       case SYS_EXIT :{
       f->eax = args[1];
       printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
       thread_exit();
  }

  case SYS_WRITE :{
      int fd = args[1];
      void *buffer = args[2];
      size_t size = args[3];

      f->eax = write(fd,buffer,size);
     

  }


  }
 
}


static int write(int fd,void * buffer, size_t size){
  if(fd == 1){
     putbuf(buffer,size);
    return size;
  }
 // return file_write(fd,buffer,size);

}
