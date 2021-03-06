#include "filesys/inode.h"

#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"







/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

void lock_inode(struct inode *inode);
void release_inode(struct inode *inode);


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{

  ASSERT (inode != NULL);
  if (pos < inode->data.length){
    if(pos < CAPACITY_OF_DIRECT * BLOCK_SECTOR_SIZE){
      int sector_num = pos / BLOCK_SECTOR_SIZE;
      return inode->data.direct[sector_num];

    }else if(pos < (CAPACITY_OF_DIRECT + CAPACITY_OF_INDIRECT) * BLOCK_SECTOR_SIZE){
        block_sector_t indirect_sector_num = (pos - CAPACITY_OF_DIRECT * BLOCK_SECTOR_SIZE) / BLOCK_SECTOR_SIZE;
        struct indirect_struct * indirect = calloc(1,sizeof(struct indirect_struct));
    
        //get indirect structure so we can get the real file location
        block_read (fs_device, inode->data.indirect, indirect);
        block_sector_t real_file_sector_num = indirect->direct[indirect_sector_num];
        free(indirect);
        return real_file_sector_num;
    }else if(pos < (CAPACITY_OF_DIRECT + CAPACITY_OF_INDIRECT + CAPACITY_OF_DOUBLE_INDIRECT) * BLOCK_SECTOR_SIZE){
        block_sector_t table_pos = (pos - (CAPACITY_OF_DIRECT * BLOCK_SECTOR_SIZE + CAPACITY_OF_INDIRECT*BLOCK_SECTOR_SIZE )) / (BLOCK_SECTOR_SIZE * CAPACITY_OF_INDIRECT);
        
        
        struct double_indirect_struct * double_indirect = calloc(1,sizeof(struct double_indirect_struct));
        block_read (fs_device, inode->data.double_indirect, double_indirect);
        
        
        struct indirect_struct * indirect = calloc(1,sizeof(struct indirect_struct));
        block_read (fs_device, double_indirect->indirect[table_pos], indirect);


        off_t before_table_bytes = table_pos*(CAPACITY_OF_INDIRECT*BLOCK_SECTOR_SIZE);
        block_sector_t indirect_pos = (pos - (CAPACITY_OF_DIRECT * BLOCK_SECTOR_SIZE + CAPACITY_OF_INDIRECT*BLOCK_SECTOR_SIZE + before_table_bytes)) / BLOCK_SECTOR_SIZE ;
        
        block_sector_t real_file_sector_num = indirect->direct[indirect_pos];
        free(indirect);
        free(double_indirect);
        
        return real_file_sector_num;
  }

  }

  return -1;

}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

int empty_slot_in_indirect_table(struct indirect_struct * table);

int empty_slot_in_indirect_table(struct indirect_struct * table){
  block_sector_t i;
  for(i=0;i<CAPACITY_OF_INDIRECT;i++){
    if(table->direct[i] == 0) return i;

  }
  return -1;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (bool is_directory,block_sector_t sector, off_t length,struct inode * inode)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);


  disk_inode = calloc (1, sizeof *disk_inode);

  memset(&disk_inode->direct,0,sizeof(disk_inode->direct));
  disk_inode->indirect = 0;
  disk_inode->double_indirect = 0;
  size_t sectors = bytes_to_sectors (length);
  if (disk_inode != NULL)
    {
      //size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_directory = is_directory;
    }  

    size_t i;
    bool indirect_structure_created = false;
    bool double_indirect_structure_created = false;
    struct indirect_struct * indirect = NULL;
    struct double_indirect_struct * double_indirect = NULL;
    bool double_indirect_table[CAPACITY_OF_INDIRECT];
    struct indirect_struct * current_indirect_struct_in_double = NULL;
    memset(double_indirect_table,0,sizeof(double_indirect_table));
    


    for(i=0;i<sectors;i++){
    
      
      if(i < CAPACITY_OF_DIRECT){

        if(free_map_allocate (1, &disk_inode->direct[i])){
        
          static char zeros[BLOCK_SECTOR_SIZE];
          block_write (fs_device, disk_inode->direct[i], zeros);
          disk_inode->direct_num++;

        }else{
      
          break;
        }
      }else if(i < CAPACITY_OF_DIRECT + CAPACITY_OF_INDIRECT){
          if(!indirect_structure_created){
          
            indirect = calloc (1, sizeof (struct indirect_struct));
            ASSERT(indirect != NULL);
            memset(indirect,0,sizeof(struct indirect_struct));
            indirect_structure_created = true;

            //allocate indirect sector on disk
            if(free_map_allocate (1, &disk_inode->indirect)){

            }else{
              break;
            }

          }  
       
                int index_of_indirect_array = i - CAPACITY_OF_DIRECT;//index in indirect array table [0..127]
                if(free_map_allocate (1, &indirect->direct[index_of_indirect_array])){
                    static char zeros[BLOCK_SECTOR_SIZE];
                    block_write (fs_device, indirect->direct[index_of_indirect_array], zeros);
                    disk_inode->indirect_num++;

                }else{
                  break;
                }
      }else{

         if(!double_indirect_structure_created){
         
          double_indirect = calloc (1, sizeof (struct double_indirect_struct));
          ASSERT(double_indirect != NULL);
          memset(double_indirect,0,sizeof(struct double_indirect_struct));
          double_indirect_structure_created = true;

          //allocate double_indirect sector on disk
          if(free_map_allocate (1, &disk_inode->double_indirect)){

          }else{
            break;
          }
     }

          
          int index_in_double_indirect_table = (i - CAPACITY_OF_DIRECT - CAPACITY_OF_INDIRECT) / CAPACITY_OF_INDIRECT;
          if(double_indirect_table[index_in_double_indirect_table] == false){
            //free previous structures before allocating new
                

              current_indirect_struct_in_double = calloc (1, sizeof (struct indirect_struct));
              ASSERT(current_indirect_struct_in_double != NULL);
              memset(current_indirect_struct_in_double,0,sizeof(struct indirect_struct));
              double_indirect_table[index_in_double_indirect_table] = true;

              //allocate indirect sector on disk and keep it's sector number in double-indirect table
              if(free_map_allocate (1, &double_indirect->indirect[index_in_double_indirect_table])){

              }else{
                break;
              }


          }

          /*now let's allocate real data sectors */
          
          int empty_slot_indirect = empty_slot_in_indirect_table(current_indirect_struct_in_double);
          //allocate space on disk for real data sector
          if(free_map_allocate (1, &current_indirect_struct_in_double->direct[empty_slot_indirect])){
                 static char zeros[BLOCK_SECTOR_SIZE];
                  block_write (fs_device, current_indirect_struct_in_double->direct[empty_slot_indirect], zeros);
                  disk_inode->double_indirect_num++;
        
          }else{
            break;
          }
      if(empty_slot_indirect == CAPACITY_OF_INDIRECT-1 || i == sectors - 1  ) {
        //write to disk indirect table which is full
                block_write (fs_device, double_indirect->indirect[index_in_double_indirect_table-1],current_indirect_struct_in_double);
                free(current_indirect_struct_in_double);
        
              }

        }
      }  
         

    

    if(i >= sectors) success = true;

    if(success){

      if(indirect_structure_created){
        //write indirect table on disk
        block_write (fs_device, disk_inode->indirect,indirect);
          
      }
      if(double_indirect_structure_created){
        block_write (fs_device, disk_inode->double_indirect,double_indirect);

      }


      //write disk_indore structure on given sector
      block_write (fs_device, sector, disk_inode);
    }

    free(disk_inode);
    if(indirect != NULL) free(indirect);
    if(double_indirect != NULL) free(double_indirect);
  
  if(inode != NULL){
  list_remove(&inode->elem);
  list_push_front (&open_inodes, &inode->elem);
  }
  
  return success;
}

int extend_file(struct inode_disk * disk_inode,off_t offset,off_t size);

int extend_file(struct inode_disk * disk_inode,off_t offset,off_t size){
  off_t length_sectors = bytes_to_sectors(disk_inode->length);
  off_t offset_sectors = bytes_to_sectors(offset + size);


  
  off_t i;
  bool indirect_structure_created = false;
  bool double_indirect_structure_created = false;
  struct indirect_struct * indirect = NULL;
  struct double_indirect_struct * double_indirect = NULL;
  bool double_indirect_table[CAPACITY_OF_INDIRECT];
  struct indirect_struct * current_indirect_struct_in_double = NULL;
  memset(double_indirect_table,0,sizeof(double_indirect_table));
  


  for(i=length_sectors;i<offset_sectors;i++){
  
    
    if(i < CAPACITY_OF_DIRECT){

      if(free_map_allocate (1, &disk_inode->direct[i])){
      
        static char zeros[BLOCK_SECTOR_SIZE];
        block_write (fs_device, disk_inode->direct[i], zeros);
        disk_inode->direct_num++;

      }else{
    
        return -1;
      }
    }else if(i < CAPACITY_OF_DIRECT + CAPACITY_OF_INDIRECT){
        if(!indirect_structure_created){
        
          indirect = calloc (1, sizeof (struct indirect_struct));
          ASSERT(indirect != NULL);
          memset(indirect,0,sizeof(struct indirect_struct));
          indirect_structure_created = true;

          if(disk_inode->indirect_num == 0){
              if(free_map_allocate (1,&disk_inode->indirect)){
                  static char zeros[BLOCK_SECTOR_SIZE];
                 block_write (fs_device, disk_inode->indirect, zeros);


              }else{
                break;
              }
          }


          block_read(fs_device,disk_inode->indirect,indirect);
        

        }



      
              int index_of_indirect_array = disk_inode->indirect_num;//index in indirect array table [0..127]
              if(free_map_allocate (1, &indirect->direct[index_of_indirect_array])){
                  static char zeros[BLOCK_SECTOR_SIZE];
                  block_write (fs_device, indirect->direct[index_of_indirect_array], zeros);
                  disk_inode->indirect_num++;

              }else{
                return -1;
              }
    }else{

        if(!double_indirect_structure_created){
        
            double_indirect = calloc (1, sizeof (struct double_indirect_struct));
            ASSERT(double_indirect != NULL);
            memset(double_indirect,0,sizeof(struct double_indirect_struct));
            double_indirect_structure_created = true;

              if(disk_inode->double_indirect_num == 0){
                  if(free_map_allocate (1,&disk_inode->double_indirect)){
                      static char zeros[BLOCK_SECTOR_SIZE];
                      block_write (fs_device,disk_inode->double_indirect, zeros);


                  }else{
                    break;
                  }
              }

            block_read(fs_device,disk_inode->double_indirect,double_indirect);

        
      }

     
        int index_in_double_indirect_table = (disk_inode->double_indirect_num) / CAPACITY_OF_INDIRECT;
        if(double_indirect_table[index_in_double_indirect_table] == false){
          //free previous structures before allocating new
              
            

            current_indirect_struct_in_double = calloc (1, sizeof (struct indirect_struct));
            ASSERT(current_indirect_struct_in_double != NULL);
            memset(current_indirect_struct_in_double,0,sizeof(struct indirect_struct));
            double_indirect_table[index_in_double_indirect_table] = true;

            if(double_indirect->indirect[index_in_double_indirect_table] == 0){
                if(free_map_allocate (1,&double_indirect->indirect[index_in_double_indirect_table])){
                      static char zeros[BLOCK_SECTOR_SIZE];
                      block_write (fs_device,double_indirect->indirect[index_in_double_indirect_table], zeros);


                }else{
                  break;
                }

            }

            block_read(fs_device,double_indirect->indirect[index_in_double_indirect_table],
                current_indirect_struct_in_double);


        }

        /*now let's allocate real data sectors */
        
        //get empty slot in current
        int empty_slot_indirect = empty_slot_in_indirect_table(current_indirect_struct_in_double);
        //allocate space on disk for real data sector
        if(free_map_allocate (1, &current_indirect_struct_in_double->direct[empty_slot_indirect])){
                static char zeros[BLOCK_SECTOR_SIZE];
                block_write (fs_device, current_indirect_struct_in_double->direct[empty_slot_indirect], zeros);
                disk_inode->double_indirect_num++;
      
        }else{
          return -1;
        }
    if(empty_slot_indirect == CAPACITY_OF_INDIRECT-1 || i == offset_sectors-1) {
      //write to disk indirect table which is full
              block_write (fs_device, double_indirect->indirect[index_in_double_indirect_table],current_indirect_struct_in_double);
              free(current_indirect_struct_in_double);
      
      }

      }


}



    if(i >= offset_sectors){

      if(indirect_structure_created){
        //write indirect table on disk
        block_write (fs_device, disk_inode->indirect,indirect);
          
      }
      if(double_indirect_structure_created){
        block_write (fs_device, disk_inode->double_indirect,double_indirect);

      }


  
    }

    if(indirect != NULL) free(indirect);
    if(double_indirect != NULL) free(double_indirect);
  
disk_inode->length = offset + size;

return 0;
}


/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {

      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
         // printf("sqqqqqq\n\n");
          if(inode->removed){
            return NULL;
          }
          inode_reopen (inode);
          return inode;
        }
    }


    
    

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);

//printf("got here\n\n");
  lock_init(&inode->inode_lock);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  if(sector  == 1){
    inode->data.is_directory = true;
  }

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{

  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  lock_inode(inode);
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
       
          free_map_release (inode->sector, 1);
        

        size_t i;
        bool has_finished_deleting = false;                    
        for(i = 0; i<CAPACITY_OF_DIRECT;i++){
          if(inode->data.direct[i] !=0){
             free_map_release (inode->data.direct[i] , 1);

          }else{
            has_finished_deleting = true;
            break;
          }
          
        }

      

        if(has_finished_deleting == false){
            struct indirect_struct * indirect = calloc(1, sizeof (struct indirect_struct));
          
            if(inode->data.indirect != 0){
                block_read (fs_device, inode->data.indirect, indirect);


                  for(i = 0;i<CAPACITY_OF_INDIRECT;i++){
                    if(indirect->direct[i] != 0){
                      free_map_release (indirect->direct[i] , 1);
                    }else{
                        has_finished_deleting = true;
                        free(indirect);
                        break;
                    } 

                  }
            }      

          free(indirect);
        }

        if(has_finished_deleting == false && inode->data.double_indirect_num !=0){
           struct double_indirect_struct * double_indirect = calloc(1, sizeof (struct double_indirect_struct));


           block_read (fs_device, inode->data.double_indirect, double_indirect);

            //iterate over double_indirect table
            for(i = 0; i<CAPACITY_OF_INDIRECT;i++){
                if(double_indirect->indirect[i] !=0){
                      struct indirect_struct * indirect = calloc(1, sizeof (struct indirect_struct));
                      block_read (fs_device, double_indirect->indirect[i], indirect);

                      size_t k;

                    for(k = 0;i<CAPACITY_OF_INDIRECT;k++){
                          if(indirect->direct[k] != 0){
                          free_map_release (indirect->direct[k] , 1);
                      }else{
                          has_finished_deleting = true;
                
                          break;
                      } 

                    }

                    free(indirect);


                  }
            }

            free(double_indirect);



        }
 
        free_map_release (inode->sector, 1);
      
      
        }
      free (inode);  
    }

    release_inode(inode);
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{

  lock_inode(inode);
  ASSERT (inode != NULL);
  inode->removed = true;
  release_inode(inode);
}



/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt) {
    return 0;
  }
  
  lock_inode(inode);
  if(offset + size > inode->data.length){
    extend_file(&inode->data,offset,size);
 
  }

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else
        {
          /* We need a bounce buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  block_write (fs_device, inode->sector, &inode->data);
  release_inode(inode);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{ 
  lock_inode(inode);
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  release_inode(inode);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  lock_inode(inode);
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
  release_inode(inode);
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
 
  return inode->data.length;
}

void lock_inode(struct inode *inode UNUSED) {}

void release_inode(struct inode *inode UNUSED) {}