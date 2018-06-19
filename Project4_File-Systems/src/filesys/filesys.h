#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "threads/thread.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
struct block *fs_device;
bool parse_path(const char * path,char * file_name,struct dir ** directory);
void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size,bool isdir);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);
void set_dir_to_root(struct thread *);

#endif /* filesys/filesys.h */
