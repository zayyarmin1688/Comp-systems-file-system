#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "directory.h"
#include "inode.h"
#include "randomfuncs.h"
#include "slist.h"

// helper function for managing string storage
// returns string after given string within data
char *process_string(char *data) {
  while (*data != 0) {
    data++;
  }
  return data + 1;
}

// initalizing the directory
void directory_init() {
  int inum = alloc_inode(); //allocate an inode number for the directory
  inode_t *rn = get_inode(inum);
  rn->mode = 040755; //set directory mode
  rn->refs = 0;
  char *selfname = ".";
  char *parentname = "..";

  directory_put(rn, selfname, inum); // add the current directory entry
  directory_put(rn, parentname, inum); //add the parent directory entry
  rn->entries = 2;
  rn->refs = 0; //resetting the ref count
}

// gets the inum of the given entry from the directory
int directory_lookup(inode_t *di, const char *name) {
  char *data = blocks_get_block(inode_get_bnum(di, 0));
  char *text = data;

  for (int i = 0; i < di->entries; ++i) {
    if (streq(text, name)) {
      text = process_string(text);
      int *inum = (int *)(text);
      return *inum;
    }

    text = process_string(text);
    text += sizeof(int);
  }
  return -ENOENT;
}

// gets the inum of the given element in the file system tree
int filesys_lookup(const char *path) {
  if (streq(path, "/")) { // Check if path is the root directory
    return 0;
  }
  path += 1; // Skip leading '/'
  
  int dir = 0; // Start at the root directory inode
  slist_t *pathlist = s_explode(path, '/'); 
  for (slist_t *tmp = pathlist; tmp != NULL; tmp = tmp->next) {
    inode_t *node = get_inode(dir); // Get the current directory inode
    dir = directory_lookup(node, tmp->data);
    if (dir == -1) {
      return -1; // this means a lack of the directory component
    }
  }
  return dir; 
}

// inserts the inum into the directory
int directory_put(inode_t *di, const char *name, int inum) {
  int nlen = strlen(name) + 1;
  if (di->size + nlen + sizeof(inum) > BLOCK_SIZE) {
    return -ENOSPC;
  }

  char *data = blocks_get_block(inode_get_bnum(di, 0));
  memcpy(data + di->size, name, nlen);
  di->size += nlen;

  memcpy(data + di->size, &inum, sizeof(inum));
  di->size += sizeof(inum);
  di->entries++;
  inode_t *sub = get_inode(inum);
  sub->refs++;
  return 0;
}

// deletes directory with the given name input
int directory_delete(inode_t *di, const char *name) {
  char *data = blocks_get_block(inode_get_bnum(di, 0));
  char *text = data;
  char *eend = 0;
  for (int i = 0; i < di->entries; i++) {
    if (strcmp(text, name) != 0) {
      text = process_string(text);
      text += 4;
    } else {
      eend = process_string(text);
      int inum = *((int *)eend);
      eend += sizeof(int);
      int epos = (int)(eend - data);
      memmove(text, eend, di->size - epos);
      int elen = (int)(eend - text);
      di->size -= elen;
      di->entries--;
      inode_t *sub = get_inode(inum);
      sub->refs--;
      if (sub->refs < 1) {
        free_inode(inum);
      }
      return 0;
    }
  }
  return -ENOENT;
}

// list all entries that was specified by the given path
slist_t *directory_list(const char *path) {
  int inum = filesys_lookup(path); // find the inode number for the given path
  inode_t *di = get_inode(inum); // get the inode object with the inode number
  char *data = blocks_get_block(inode_get_bnum(di, 0));
  char *text = data;

  slist_t *dir_list_wip = NULL; // initialize empty list to store directory entries

  for (int i = 0; i < di->entries; i++) {
    char *name = text;
    text = process_string(text);
    text += sizeof(int);       // go to next directory entry

    dir_list_wip = s_cons(name, dir_list_wip); // add entry to list
  }
  return dir_list_wip;
}

// prints the items in the directory specified by path
void print_directory(const char *path) {
  printf("Contents:\n");
  slist_t *items = directory_list(path);
  for (slist_t *xs = items; xs != 0; xs = xs->next) {
    printf("- %s\n", xs->data);
  }
  printf("(end of contents)\n");
  s_free(items);
}
