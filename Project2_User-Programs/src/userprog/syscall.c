#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "list.h"
#include "process.h"



static bool is_valid_addr(const uint32_t *uaddr);
static void validate_uaddr(const uint32_t *uaddr);
static void syscall_handler (struct intr_frame *);
static int write(int fd,void * buffer, size_t size);
static bool create (const char* file, unsigned initial_size);
static void exit(int code);







void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}



static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);
   if (!is_valid_addr((void*)args)) {
    exit(-1);
  }

  int num_syscall = args[0];

 
  switch(num_syscall) {

    case SYS_HALT: {

      break;
    }

      break;


    case SYS_EXIT: {

      validate_uaddr((void*)(args + 1));
      
      exit(args[1]);
      break;
    }

    case SYS_EXEC: {

      break;
    }

    case SYS_WAIT: {

      break;
    }

    case SYS_CREATE: {
      char * file = (char*)args[1];
      unsigned  size = args[2];


      f->eax = create(file,size);
      break;
    
    }

    case SYS_REMOVE: {

      break;

    }

    case SYS_OPEN: {

      break;
    }

    case SYS_FILESIZE: {

      break;
    }

    case SYS_READ: {

      break;
    }

    case SYS_WRITE: {
      int fd = args[1];
      void *buffer = (void*)args[2];
      size_t size = args[3];

      f->eax = write(fd,buffer,size);
      break;
    }
  

    case SYS_SEEK: {

      break;
    }

    case SYS_TELL: {

      break;
    }

    case SYS_CLOSE: {

      break;
    }

  }
}


static int write(int fd,void * buffer, size_t size) {
  if(fd == 1){
     putbuf(buffer,size);
    return size;
  }
 // return file_write(fd,buffer,size);
  return 0;
}


static void exit(int code) {
  printf("%s: exit(%d)\n", thread_current()->name, code);
  thread_exit();
}


static bool create (const char* file, unsigned initial_size) {

  validate_uaddr((uint32_t*)file);
  
  lock_acquire(get_file_system_lock());
//using synchronization constructs:

  bool res = filesys_create (file,  initial_size);
  lock_release(get_file_system_lock());

  return res;
}

static bool is_valid_addr(const uint32_t *uaddr) {
  if (is_user_vaddr(uaddr) && is_user_base_correct(uaddr)) {
    return (pagedir_get_page(thread_current()->pagedir, uaddr) != NULL);
  }
  return false;
}

static void validate_uaddr(const uint32_t *uaddr) {
  if (!is_valid_addr(uaddr))
    exit(-1);
}