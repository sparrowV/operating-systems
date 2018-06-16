#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);


bool parse_path(char * path,char * file_name,struct dir * directory){


  ASSERT(thread_current()->process_directory != NULL);

  /*
  if(!(path[0] == '.' || path[0] == '/')){
    printf("file name is %s",path);
    printf("secotr num is == %d",thread_current()->process_directory->inode->sector);
     memcpy(directory,thread_current()->process_directory,sizeof(struct dir));
 
     strlcpy(file_name,path,strlen(path)+1);
  
     return true;
  }
*/
  //for absolute path

    struct dir * root;
    if(path[0] == '/'){
      memcpy(directory,dir_open_root(),sizeof(struct dir));

      root = dir_open_root();
    }else{
       memcpy(directory,thread_current()->process_directory,sizeof(struct dir));
       root = thread_current()->process_directory;
       //printf("current dir is %d = \n ",root->inode->sector);
    }
   
    char * temp = malloc(strlen(path)+1);
    memset(temp,0,strlen(path)+1);
 
    size_t i = 0;
    size_t temp_index = 0;
    
    while(path[i]!='\0'){
        if(path[i] == '/' && i!=0 ){
          struct inode * dir_inode = NULL;
          if(!dir_lookup(root,temp,&dir_inode)){
  
              return false;
          }
          if(!dir_inode->data.is_directory) {
            return false;
          }
          directory->inode = dir_inode;
          root = directory;
             
           memset(temp,0,strlen(path)+1);
           temp_index = 0;
           
        }else if(path[i] != '/' || i!=0){

            temp[temp_index] = path[i];
            temp_index++;
            
        }
        i++;
        
    }

    strlcpy(file_name,temp,strlen(temp)+1);
    return true;


}



/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size, bool is_dir)
{

  block_sector_t inode_sector = 0;
//struct dir *dir = dir_open_root ();
  struct dir * dir = malloc(sizeof(struct dir));
  char * file_name = malloc(strlen(path)+1);
 

   if(strlen(path) > NAME_MAX){
    return false;
  }

  bool path_found = parse_path(path,file_name,dir);
  
  struct inode * file_inode = NULL;
  dir_lookup(dir,file_name,&file_inode);

  if(!path_found || file_inode != NULL){
    dir_close(dir);
    free(file_name);
    return false;
  }
  
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (is_dir, inode_sector, initial_size)
                  && dir_add (dir, file_name, inode_sector));


  struct inode * dir_inode = NULL;
  dir_lookup(dir,file_name,&dir_inode);
  

  if(dir_inode == NULL){

    return false;
  }

  struct dir * newly_created_dir = malloc(sizeof(struct dir));
  newly_created_dir->inode = dir_inode;
  newly_created_dir->pos = 0;

   
  if(is_dir){
    
    dir_add(newly_created_dir,".",inode_sector);
    dir_add(newly_created_dir,"..",dir->inode->sector);
    
  }  


  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  //free(dir);
  //free(file_name);

  return success;

}

 


/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
    
    /*
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);
  */
 struct inode *inode = NULL;
  struct dir * dir = malloc(sizeof(struct dir));
  char * file_name = malloc(strlen(name)+1);

  bool found_path = parse_path(name,file_name,dir);

  if(!found_path){
    //free(dir);
    dir_close(dir);
    free(file_name);
    return NULL;
  }

 // printf("in file open dir is %d=\n",dir->inode->sector);
  if(!dir_lookup(dir,file_name,&inode)){
    //printf("cant find file %s=",file_name);

   dir_close (dir);
   free(file_name);
   return NULL;
  }
  
  if(inode->data.is_directory)
    inode->deny_write_cnt = 1;
 
  ASSERT(inode != NULL);
  return file_open (inode);

}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{

  /*
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir);

  return success;
  */
  struct inode *inode = NULL;
  struct dir * dir = malloc(sizeof(struct dir));
  char * file_name = malloc(strlen(name)+1);

  if(!parse_path(name,file_name,dir)){
    return false;

  }

  if(strlen(file_name) == 0){
    return false;
  }

  dir_lookup(dir,file_name,&inode);

  if(inode->data.is_directory){
    struct dir *child_dir = dir_open(inode);



    int count = count_dir_childs(child_dir);
    dir_close(child_dir);
    if (count > 0){
      dir_close(dir);
      return false;
    }
    


  }

  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir);

  return success;






}

void set_dir_to_root(struct thread *t) {
  block_sector_t sector = 1;

  struct inode * inode= malloc (sizeof(struct inode));

  inode->sector = sector;
  inode->open_cnt = 0;
  inode->deny_write_cnt = 0;
  inode->removed = false;
 

  struct dir * dir= malloc(sizeof(struct dir));
  dir->inode = inode;
  dir->inode->data.is_directory = true;

  t->process_directory = dir;
  printf("set dir to root name is = %s  ",thread_current()->name);
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  set_dir_to_root(thread_current()); 
  free_map_close ();
  printf ("done.\n");
}
