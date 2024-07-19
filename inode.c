#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "inode.h"

const int INODES = 64;

// print information about certain inode
void print_inode(inode_t *node) {
  if (node) {
    printf("node{refs: %d, mode: %04o, size: %d, entries: %d, pointers[0]: %d, "
           "pointers[1]: %d, indirect pointer: %d}\n",
           node->refs, node->mode, node->size, node->entries, node->pointers[0],
           node->pointers[1], node->indir_point);
  } else {
    printf("null node\n"); //case where node is null
  }
}

// find inode of certain number within memory
inode_t *get_inode(int inum) {
  //printf("-=== CHECKING INODE: %i ===-\n", inum);
  // The one line that screwed things up for hours, FUN!
  uint8_t *base = get_inode_bitmap();
  inode_t *nodes = (inode_t *)(base);
  return &(nodes[inum]); //return the requested inode
}

// create and allocate space for a new inode
int alloc_inode() {
  for (int i = 0; i < INODES; i++) {
    inode_t *node = get_inode(i);
    if (node->mode == 0) { //check if the inode is free
      time_t now = time(NULL);
      memset(node, 0, sizeof(inode_t)); //checking the structure
      node->refs = 0;
      node->mode = 010644;
      node->size = 0;
      node->entries = 0;
      node->pointers[0] = alloc_block(); //allocating the first block
      node->pointers[1] = 0;
      node->indir_point = 0;
      node->create_time = now;
      node->acc_time = now;
      node->mod_time = now;
      printf("+ alloc_inode() -> %d\n", i);
      return i;
    }
  }
  return -1;
}

// free space after inode is no longer needed
void free_inode(int inum) {
  printf("+ free_inode(%d)\n", inum);
  inode_t *node = get_inode(inum);
  if (node->refs <= 0) { 
    shrink_inode(node, 0); //reduce the inode to size 0
    free_block(node->pointers[0]);
    memset(node, 0, sizeof(inode_t)); //clearing inode struct
  } else {
    puts("Cannot free inode!");
    abort();
  }
}

// increase space allocated for inode
int grow_inode(inode_t *node, int size) {
  int numBlocks = (node->size / BLOCK_SIZE) + 1;
  int neoNumBlocks = (size / BLOCK_SIZE) + 1;
  while (numBlocks < neoNumBlocks) {
    //if needed this allocates additional blocks
    if (node->pointers[1] == 0) {
      node->pointers[1] = alloc_block();
    } else {
      
      if (node->indir_point == 0) {
        node->indir_point = alloc_block();
      }
      int *ipointers = blocks_get_block(node->indir_point);
      ipointers[numBlocks - 1] = alloc_block();
    }
    numBlocks += 1;
  }
  node->size = size;
  return 0;
}

// decrease space allocated for inode
int shrink_inode(inode_t *node, int size) {
  int numBlocks = (node->size / BLOCK_SIZE) + 1;
  int neoNumBlocks = (size / BLOCK_SIZE) + 1;
  while (numBlocks > neoNumBlocks) {
    //free uneeded blocks
    int bnum = inode_get_bnum(node, node->size);
    free_block(bnum);
    if (node->indir_point) {
      int *ipointers = blocks_get_block(node->indir_point);
      int iptrIndex = (node->size / BLOCK_SIZE) - 2;
      ipointers[iptrIndex] = 0;
      if (ipointers[0] == 0) {
        node->indir_point = 0;
      }
    } else if (node->pointers[1]) {
      node->pointers[1] = 0;
    } else {
      puts("Attempted to shrink last remaining block");
      return -1;
    }
    numBlocks -= 1;
  }
  return 0;
}

// get block number for given inode
int inode_get_bnum(inode_t *node, int file_bnum) {
  int blockIndex = file_bnum / BLOCK_SIZE;
  if (blockIndex < 2) {
    return node->pointers[blockIndex];
  } else {
    int *indir_point = blocks_get_block(node->indir_point);
    return indir_point[blockIndex - 2];
  }
}
