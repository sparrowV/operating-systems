#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "threads/vaddr.h"
#include "pagedir.h"
#include "list.h"
#include "process.h"




static void syscall_handler (struct intr_frame *);
static int write(int fd,void * buffer, size_t size);
static int exit(int code){
   printf("%s: exit(%d)\n", &thread_current ()->name, code);
    thread_exit();
}
//static void exit(int status);

//
static bool is_valid_addr(const uint8_t *uaddr) {
  if (is_user_vaddr(uaddr) && is_user_base_correct(uaddr)) {
    return (pagedir_get_page(thread_current()->pagedir, uaddr) != NULL);
  }
//  printf("sd\n\n");
  return false;
}

static bool create (const char* file, unsigned initial_size)
{

  if(!is_valid_addr(file)){
    exit(-1);
  }
  lock_acquire(get_file_system_lock());
//using synchronization constructs:

bool res = filesys_create (file,  initial_size);
lock_release(get_file_system_lock());

return res;
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
  //if(!(is_user_vaddr(args)&& is_user_base_correct(args))) {
   if (!is_valid_addr(args)) {
    exit(-1);
  }
  int num_syscall = args[0];

  // if (args[0] > PHYS_BASE){
  //   thread_exit();
  // }
  // 
  
 

  //printf("System call number: %d\n", args[0]);

  switch(num_syscall){
       case SYS_EXIT :{
       //f->eax = args[1];
      // printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
      if(!is_valid_addr(args + 1)){
        exit(-1);
      }else{
        exit(args[1]);
      }
      break;

  }

  case SYS_WRITE :{
      int fd = args[1];
      void *buffer = args[2];
      size_t size = args[3];

      f->eax = write(fd,buffer,size);
      break;
     

  }
  
  case SYS_CREATE :{
      char * file = args[1];
      unsigned  size = args[2];


      f->eax = create(file,size);
      break;
     

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
