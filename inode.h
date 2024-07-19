// Inode manipulation routines.
//
// Feel free to use as inspiration. Provided as-is.

// based on cs3650 starter code
#ifndef INODE_H
#define INODE_H

#include <stdio.h>
#include <time.h>

#include "blocks.h"

extern const int BLOCK_COUNT;
extern const int BLOCK_SIZE;
extern const int NUFS_SIZE;
extern const int BLOCK_BITMAP_SIZE;

typedef struct inode {
  int refs; // reference count
  int mode; // permission & type
  int size; // bytes
  int entries;
  int pointers[2];
  int indir_point;
  time_t create_time;
  time_t acc_time;
  time_t mod_time;
} inode_t;

void print_inode(inode_t *node);
inode_t *get_inode(int inum);
int alloc_inode();
void free_inode(int inum);
int grow_inode(inode_t *node, int size);
int shrink_inode(inode_t *node, int size);
int inode_get_bnum(inode_t *node, int file_bnum);

#endif
