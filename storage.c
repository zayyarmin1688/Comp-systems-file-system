#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "blocks.h"
#include "directory.h"
#include "inode.h"
#include "randomfuncs.h"
#include "slist.h"
#include "storage.h"

// initializes storage
void storage_init(const char *path) {
  blocks_init(path);
  directory_init();
}

// logic for nufs getattr
int storage_stat(const char *path, struct stat *st) {
  int inode_number = filesys_lookup(path);
  printf("+ storage_stat(%s) -> 0; inode %d\n", path, inode_number);
  if (inode_number >= 0) {
    inode_t *node = get_inode(inode_number);
    print_inode(node);
    memset(st, 0, sizeof(struct stat));
    st->st_uid = getuid();
    st->st_mode = node->mode;
    st->st_size = node->size;
    st->st_nlink = node->refs;
    return 0;
  } else {
    return inode_number;
  }
}

// reads {size} bytes from path contents to buffer
int storage_read(const char *path, char *buf, size_t size, off_t offset) {
  int inode_number = filesys_lookup(path);
  if (inode_number < 0) {
    return inode_number;
  }

  inode_t *node = get_inode(inode_number);
  printf("+ storage_read(%s); inode %d\n", path, inode_number);
  print_inode(node);

  if (offset >= node->size) {
    return 0;
  }

  size = min(size, node->size - offset);
  int num_read = 0;

  while (num_read < size) {
    int block_num = inode_get_bnum(node, offset + num_read);
    char *starting_block = blocks_get_block(block_num);
    char *read_ptr = starting_block + ((offset + num_read) % BLOCK_SIZE);

    int bytes_to_read =
        min(size - num_read, BLOCK_SIZE - ((offset + num_read) % BLOCK_SIZE));
    printf(" + reading from block: %d\n", block_num);
    memcpy(buf + num_read, read_ptr, bytes_to_read);
    num_read += bytes_to_read;
  }

  return num_read;
}

// writes {size} bytes from buffer to path contents
int storage_write(const char *path, const char *buf, size_t size,
                  off_t offset) {
  /*
  printf("-=-=-=-=-=-=- ATTEMPTING STORAGE WRITE -=-=-=-=-=-=-\n");
  printf("PATH: %s\n", path);
  printf("BUFFER: %s\n", buf);
  printf("SIZE: %li\n", size);
  printf("OFFSET: %li\n", offset);
  printf("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
  //*/
  int truncate_result = storage_truncate(path, offset + size);
  if (truncate_result < 0) {
    return truncate_result;
  }

  int inode_number = filesys_lookup(path);
  if (inode_number < 0) {
    return inode_number;
  }

  inode_t *node = get_inode(inode_number);
  printf("+ storage_write(%s); inode %d\n", path, inode_number);
  print_inode(node);

  int num_written = 0;

  while (num_written < size) {
    int block_num = inode_get_bnum(node, offset + num_written);
    char *starting_block = blocks_get_block(block_num);
    char *write_ptr = starting_block + ((offset + num_written) % BLOCK_SIZE);

    int bytes_to_write = min(
        size - num_written, BLOCK_SIZE - ((offset + num_written) % BLOCK_SIZE));
    printf("+ writing to block: %d\n", block_num);
    memcpy(write_ptr, buf + num_written, bytes_to_write);
    num_written += bytes_to_write;
  }

  return num_written;
}

// changes length of file by calling grow/shrink inode
int storage_truncate(const char *path, off_t size) {
  int inode_number = filesys_lookup(path);
  if (inode_number < 0) {
    return inode_number;
  }

  inode_t *node = get_inode(inode_number);
  if (size >= node->size) {
    return grow_inode(node, size);
  } else {
    return shrink_inode(node, size);
  }
}

// Parse full path for...
// ...Parent of given file
void storage_find_parent(const char *fullpath, char *dir) {
  strcpy(dir, fullpath);

  int word_len = 0;
  for (int i = strlen(fullpath) - 1; fullpath[i] != '/'; --i) {
    word_len++;
  }

  if (word_len == strlen(fullpath) - 1) {
    dir[1] = '\0';
  } else {
    dir[strlen(fullpath) - word_len - 1] = '\0';
  }
}

// ...Child of given file
char *storage_find_child(const char *fullpath, char *sub) {
  strcpy(sub, fullpath);

  int word_len = 0;
  for (int i = strlen(fullpath) - 1; fullpath[i] != '/'; i--) {
    word_len++;
  }

  sub += strlen(fullpath) - word_len;
  return sub;
}

// create new node (file or dir) at given path
int storage_mknod(const char *path, int mode) {
  if (filesys_lookup(path) > -1) {
    printf("Node already exists!\n");
    return -EEXIST;
  }

  char *newNodePath = malloc(strlen(path) + 10);
  memset(newNodePath, 0, strlen(path) + 10);
  newNodePath[0] = '/'; // root

  slist_t *items = s_explode(path + 1, '/'); // path as list of strings
  for (slist_t *currentItem = items; currentItem != NULL; currentItem = currentItem->next) {
    int s = directory_lookup(get_inode(filesys_lookup(newNodePath)), currentItem->data);
    int d = filesys_lookup(newNodePath);
    if (currentItem->next == NULL && s < 0) { // end of the pathway
      int nodeNumToCreate = alloc_inode(); // create new node
      inode_t *created = get_inode(nodeNumToCreate);
      created->mode = mode;

      if (created->mode == 040755) {
        char *selfname = "."; // setting system default directories
        char *parentname = "..";

        directory_put(created, parentname, filesys_lookup(newNodePath));
        directory_put(created, selfname, nodeNumToCreate);
      }

      int rv = directory_put(get_inode(filesys_lookup(newNodePath)), currentItem->data,
                             nodeNumToCreate);
      if (rv != 0) {
        printf("Couldn't make node at %s!", newNodePath);
        free(newNodePath);
        return -1;
      }

      created->refs = 1;
      free(newNodePath);
      return 0;
    } else if (s > -1 && get_inode(s)->mode == 040755) { // intermediate node
      if (strcmp(newNodePath, "/") == 0) {
        strcpy(newNodePath + strlen(newNodePath), currentItem->data);
      } else {
        strcpy(newNodePath + strlen(newNodePath), "/");
        strcpy(newNodePath + strlen(newNodePath), currentItem->data);
      }
    } else { // root node
      int nodeNumToCreate = alloc_inode();
      inode_t *created = get_inode(nodeNumToCreate);
      created->mode = 040755;

      char *selfname = ".";
      char *parentname = "..";

      directory_put(created, parentname, d);
      directory_put(created, selfname, nodeNumToCreate);
      directory_put(get_inode(d), currentItem->data, nodeNumToCreate);
      created->refs = 1;

      if (strcmp(newNodePath, "/") == 0) { // if new node is base node
        strcat(newNodePath, currentItem->data);
      } else {
        strcat(newNodePath, "/");
        strcat(newNodePath, currentItem->data);
      }
      // loop the loop again!
    }
  }
}

// unlinks node at given path from parent
int storage_unlink(const char *path) {
  char *dir = alloca(strlen(path) + 1);
  char *sub = alloca(strlen(path) + 1);

  storage_find_parent(path, dir);
  sub = storage_find_child(path, sub);

  int dirnum = filesys_lookup(dir);
  int subnum = filesys_lookup(path);

  inode_t *dirnode = get_inode(dirnum);
  inode_t *subnode = get_inode(subnum);

  int rv = directory_delete(dirnode, sub);
  return rv;
}

// link files {from} and {to}
int storage_link(const char *from, const char *to) {
  int toNum = filesys_lookup(to);
  if (toNum >= 0) {
    char *fromParent = alloca(strlen(from) + 1);
    char *fromChild = alloca(strlen(from) + 1);
    storage_find_parent(from, fromParent);
    fromChild = storage_find_child(from, fromChild);

    int fromParentNum = filesys_lookup(fromParent);
    inode_t *fromParentNode = get_inode(fromParentNum);

    int rv = directory_put(fromParentNode, fromChild, toNum);
    return rv;
  } else {
    return toNum;
  }
}

// renames file named {from} to {to}
int storage_rename(const char *from, const char *to) {
  int link_result = storage_link(to, from);
  if (link_result >= 0) {
    return storage_unlink(from);
  } else {
    return link_result;
  }  
}

// updates timestamps of a file within storage
int storage_set_time(const char *path, const struct timespec ts[2]) {
  int inode_number = filesys_lookup(path);
  if (inode_number >= 0) {
    inode_t *node = get_inode(inode_number);
    node->mod_time = ts->tv_sec;
    return 0;
  } else {
    return inode_number;
  }
}

// returns 0 if file at path can be accessed
int storage_can_find(const char *path) {
  int inode_number = filesys_lookup(path);
  if (inode_number >= 0) {
    inode_t *node = get_inode(inode_number);
    node->acc_time = time(NULL);
    return 0;
  } else {
    return -1;
  }
}
