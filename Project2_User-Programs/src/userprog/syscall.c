#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "list.h"
#include "process.h"
#include "devices/shutdown.h"
#include "devices/input.h"


//bool is_valid_addr(const void *uaddr);
static void validate_uaddr(const void *uaddr);
static void syscall_handler (struct intr_frame *);
static int write(int fd,void * buffer, size_t size);
static bool create (const char* file, unsigned initial_size);
//static void open(struct file_desc * open_files);
//static void exit(int code);
static int get_file_desc(struct file_desc * file_descs);
bool is_valid_fd(int fd);
static int read(int fd,uint8_t  * buffer, size_t size);



void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}



static void
syscall_handler (struct intr_frame *f UNUSED)
{
  uint32_t* args = ((uint32_t*) f->esp);
  validate_uaddr(args);

  int num_syscall = args[0];

 
  switch(num_syscall) {

    case SYS_HALT: {
      shutdown_power_off();
      break;
    }


    case SYS_EXIT: {

      validate_uaddr(args + 1);
      
      exit(args[1]);
      break;
    }

    case SYS_EXEC: {
      validate_uaddr(args+1);
      validate_uaddr(args[1]);
      char *cmd_line = (char*)args[1];
      char * arr = malloc(strlen(cmd_line) + 4);
      memset(arr,0,strlen(cmd_line)+4);
    int i=0;
    while(cmd_line[i] != ' ' && i<strlen(cmd_line)){
      arr[i] = cmd_line[i];
        i++;
    }
  /* lock_acquire(get_file_system_lock());
    struct file* cmd_file = filesys_open (arr);
    if(cmd_file == NULL){
	lock_release(get_file_system_lock());
      f->eax = -1;

    }else{
      file_close(cmd_file);
      lock_release(get_file_system_lock()); */
      f->eax = process_execute(cmd_line);
    
 	  
      break;
    }

    case SYS_WAIT: {
      validate_uaddr(args+1);

      tid_t id = args[1];
      f->eax = process_wait(id);
      break;
    }

    case SYS_CREATE: {
      char * file = (char*)args[1];
      unsigned  size = args[2];


      f->eax = create(file,size);
      break;
    
    }

    case SYS_REMOVE: {
      validate_uaddr(args+1);
      char *file_name = (char*)args[1];
      validate_uaddr(file_name);
      lock_acquire(get_file_system_lock());
      f->eax = filesys_remove(file_name);
      lock_release(get_file_system_lock());

      break;

    }

    case SYS_OPEN: {
      validate_uaddr(args+1);
      char *file_name = (char*)args[1];
      validate_uaddr(file_name);

      struct file_desc * open_files = (struct file_desc*)&(thread_current()->file_descs);
      
      int fd = get_file_desc(open_files);
      struct file_desc *cur = &open_files[fd];
      lock_acquire(get_file_system_lock());
      cur->open_file = filesys_open(file_name);
      lock_release(get_file_system_lock());
      if (cur->open_file != NULL) {
            cur->is_open = true;
            f->eax = fd;
          } else {
            f->eax = -1;
          }
      break;
    }

    case SYS_FILESIZE: {
      validate_uaddr(args+1);
      int fd = args[1];

		  lock_acquire(get_file_system_lock());

      struct file *cur_file = thread_current()->file_descs[fd].open_file;
      f->eax = file_length (cur_file);
      lock_release(get_file_system_lock());
  
        break;
      }

    case SYS_READ: {
      validate_uaddr(args+1);
      validate_uaddr(args+2);
      validate_uaddr(args+3);


      int fd = args[1];
      uint8_t *buffer = (uint8_t *)args[2];
      size_t size = args[3];
    

      validate_uaddr(buffer);

      lock_acquire(get_file_system_lock());
      int res = read(fd,buffer,size);
      if (res == -1) {
        lock_release(get_file_system_lock());
        exit(-1);
      }
      f->eax = res;
      lock_release(get_file_system_lock());



      break;
    }

    case SYS_WRITE: {
      validate_uaddr(args+1);
      validate_uaddr(args+2);

      validate_uaddr(args+3);
      int fd = args[1];
      void *buffer = (void*)args[2];
      size_t size = args[3];
      validate_uaddr(buffer);
    

      lock_acquire(get_file_system_lock());
      int res = write(fd,buffer,size);
          f->eax = res;
      lock_release(get_file_system_lock());
      break;
    }
  

    case SYS_SEEK: {
      validate_uaddr(args+1);
      validate_uaddr(args+2);
      int fd = args[1];
      int new_pos = args[2];
      lock_acquire(get_file_system_lock());
      struct file *cur_file = thread_current()->file_descs[fd].open_file;
      file_seek(cur_file,new_pos);
      lock_release(get_file_system_lock());


      break;
    }

    case SYS_TELL: {
      validate_uaddr(args+1);
      lock_acquire(get_file_system_lock());
      int fd = args[1];
      struct file *cur_file = thread_current()->file_descs[fd].open_file;
      f->eax =(cur_file->pos + 1);

      break;
    }

    case SYS_CLOSE: {
      validate_uaddr(args+1);
      int fd = args[1];
      struct file_desc *cur = &thread_current()->file_descs[fd];
        if (is_valid_fd(fd) && cur->is_open) {
          lock_acquire(get_file_system_lock());
          file_close(cur->open_file);
          lock_release(get_file_system_lock());
          cur->is_open = false;
        }
      break;
    }

    case SYS_PRACTICE: {
      validate_uaddr(args+1);

      int res = args[1] + 1;
      f->eax = res;
      break;
    }

  }
}

static int read(int fd,uint8_t  * buffer, size_t size) {
  if (!is_valid_fd(fd)) return -1;
  if (fd == 0){
    
    size_t index = 0;
    
    for(;index < size; index++){
     	buffer[index] = input_getc(); 
    }
   
    return size;
  } else {
    struct file *cur_file = thread_current()->file_descs[fd].open_file;
    if(cur_file == NULL) return -1;
 
      int s =  file_read(cur_file,buffer,size);
      return s;
    
    
  }

}


static int write(int fd,void * buffer, size_t size) {
  //printf("%d\n\n\n",fd);
  if (!is_valid_fd(fd)) return -1;
  if (fd == 1){
     putbuf(buffer,size);
    return size;
  } else {
    struct file *cur_file = thread_current()->file_descs[fd].open_file;
    if(cur_file == NULL) return -1;
        //thread_yield();
        //printf("%s\n\n",(char *)buffer);
       // printf("id is %d",thread_current()->tid);
      return file_write(cur_file,buffer,size);
    
  }
  return -1;
}


void exit(int code) {
  int i = 0;
  bool iswaited= false;
  struct thread *parent = thread_current()->parent;
 
  if (parent != NULL) {
    for (; i < MAX_CHILDREN; ++i) {
      if (parent->child_arr[i].id == thread_current()->tid) {
        //printf("exiting thread %s parent is %s",thread_current()->name,parent->name);
        parent->child_arr[i].exit_status = code;
		parent->child_arr[i].already_exited = true;
        break;
      }
    }
  }

   file_close(thread_current()->threads_exec_file);
  thread_current()->st = code;
  if (parent->waiting_on_thread == thread_current()->tid) {
    sema_up(&parent->wait_for_child);
  } 
  
  printf("%s: exit(%d)\n", thread_current()->name, code);
  thread_exit();
}


static bool create (const char* file, unsigned initial_size) {

  validate_uaddr(file);
  
  lock_acquire(get_file_system_lock());
//using synchronization constructs:

  bool res = filesys_create (file,  initial_size);
  lock_release(get_file_system_lock());

  return res;
}

bool is_valid_uaddr(const void *uaddr) {
  if (is_user_vaddr(uaddr) && is_user_base_correct(uaddr)) {
    return (pagedir_get_page(thread_current()->pagedir, uaddr) != NULL);
  }
  return false;
}

static void validate_uaddr(const void *uaddr) {
  if (!is_valid_uaddr(uaddr))
    exit(-1);
}

static int get_file_desc(struct file_desc * file_descs) {
  int i = 0;
  for (; i < MAX_OPEN_FILES; i++) {
    if (!file_descs[i].is_open) {
      return i;
    }
  }
  return i;
}

bool is_valid_fd(int fd) {
  return (fd >= 0 && fd <= MAX_OPEN_FILES);
}