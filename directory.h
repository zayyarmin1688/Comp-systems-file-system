// Directory manipulation functions.
//
// Feel free to use as inspiration. Provided as-is.

// Based on cs3650 starter code
#ifndef DIRECTORY_H
#define DIRECTORY_H

#define DIR_NAME_LENGTH 48

#include "blocks.h"
#include "inode.h"
#include "slist.h"

extern const int BLOCK_COUNT;
extern const int BLOCK_SIZE;
extern const int NUFS_SIZE;
extern const int BLOCK_BITMAP_SIZE;

void directory_init();
int directory_lookup(inode_t *di, const char *name);
int directory_put(inode_t *di, const char *name, int inum);
int directory_delete(inode_t *di, const char *name);
int filesys_lookup(const char *path);
slist_t *directory_list(const char *path);
void print_directory(const char *path);
char *process_string(char *data);

#endif
