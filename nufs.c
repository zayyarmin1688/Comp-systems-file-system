// based on cs3650 starter code

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "directory.h"
#include "storage.h"

const int XS_CONST = 126;

// implementation for: man 2 access
// Checks if a file exists.
int nufs_access(const char *path, int mask) {
  int rv = storage_can_find(path);

  printf("access(%s, %04o) -> %d\n", path, mask, rv);
  return rv;
}

// Gets an object's attributes (type, permissions, size, etc).
// Implementation for: man 2 stat
// This is a crucial function.
int nufs_getattr(const char *path, struct stat *st) {
  int rv = storage_stat(path, st);

  printf("getattr(%s) -> (%d) {mode: %04o, size: %ld}\n", path, rv, st->st_mode,
         st->st_size);
  return rv;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
  struct stat st;
  char item_path[128];
  int rv;

  slist_t *items = directory_list(path);
  for (slist_t *xs = items; xs != 0; xs = xs->next) {
    printf("+ viewing path: '%s'\n", xs->data);
    if (strcmp(path, "/") == 0) {
      item_path[0] = '/';

      int xs_data_len = strlen(xs->data);
      strncpy(item_path + 1, xs->data, XS_CONST);
      if (xs_data_len >= XS_CONST) {
        xs_data_len = XS_CONST;
      }
      item_path[xs_data_len + 1] = '\0';
    } else {
      strncpy(item_path, path, strlen(path));
      item_path[strlen(path)] = '/';

      int xs_data_len = strlen(xs->data);
      strncpy(item_path + strlen(path) + 1, xs->data, XS_CONST);
      if (xs_data_len >= XS_CONST) {
        xs_data_len = XS_CONST;
      }
      item_path[xs_data_len + strlen(path) + 1] = '\0';
    }

    rv = storage_stat(item_path, &st);
    filler(buf, xs->data, &st, 0);
  }
  s_free(items);

  printf("readdir(%s) -> %d\n", path, rv);
  return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
// Note, for this assignment, you can alternatively implement the create
// function.
int nufs_mknod(const char *path, mode_t mode, dev_t rdev) {
  int rv = storage_mknod(path, mode);
  printf("mknod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int nufs_mkdir(const char *path, mode_t mode) {
  int rv = nufs_mknod(path, mode | 040000, 0);
  printf("mkdir(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_unlink(const char *path) {
  int rv = storage_unlink(path);
  printf("unlink(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_link(const char *from, const char *to) {
  int rv = storage_link(to, from);
  printf("link(%s => %s) -> %d\n", from, to, rv);
  return rv;
}

int nufs_rmdir(const char *path) {
  inode_t *node = get_inode(filesys_lookup(path));
  int mode = node->mode;
  if (mode != 040755) {
    printf("rmdir(%s) -> %d\n", path, -1);
    return -1;
  }

  char *data = blocks_get_block(node->pointers[0]);
  char *text = data;

  for (int i = 0; i < node->entries; i++) {
    // alloca?
    char *path_full = malloc(strlen(path) + strlen(text) + 2);
    // Check memory allocation
    if (!path_full) {
      printf("Error allocating path memory\n");
      return -1;
    }
    char *pos = path_full;
    memcpy(pos, path, strlen(path));
    pos += strlen(path);
    if (strcmp(path, "/") != 0) {
      pos[0] = '/';
      pos += 1;
    }
    memcpy(pos, text, strlen(text) + 1);

    nufs_unlink(path_full);
    text = process_string(text);
    text += 4;
  }
  int rv = nufs_unlink(path);
  printf("rmdir(%s) -> %d\n", path, rv);
  return rv;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char *from, const char *to) {
  int rv = storage_rename(from, to);
  printf("rename(%s => %s) -> %d\n", from, to, rv);
  return rv;
}

int nufs_chmod(const char *path, mode_t mode) {
  int rv = -1;
  int inum = filesys_lookup(path);
  inode_t *node = get_inode(inum);

  if (node->mode == mode) {
    return 0;
  } else {
    shrink_inode(node, 0);
    node->mode = mode;
    rv = 0;
  }

  printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

int nufs_truncate(const char *path, off_t size) {
  int rv = storage_truncate(path, size);
  printf("truncate(%s, %ld bytes) -> %d\n", path, size, rv);
  return rv;
}

// This is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
// You can just check whether the file is accessible.
int nufs_open(const char *path, struct fuse_file_info *fi) {
  int rv = nufs_access(path, 0);
  printf("open(%s) -> %d\n", path, rv);
  return rv;
}

// Actually read data
int nufs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
  int rv = storage_read(path, buf, size, offset);
  printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
  return rv;
}

// Actually write data
int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
  int rv = storage_write(path, buf, size, offset);
  printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
  return rv;
}

// Update the timestamps on a file or directory.
int nufs_utimens(const char *path, const struct timespec ts[2]) {
  int rv = storage_set_time(path, ts);
  printf("utimens(%s, [%ld, %ld; %ld %ld]) -> %d\n", path, ts[0].tv_sec,
         ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, rv);
  return rv;
}

// Extended operations
int nufs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
               unsigned int flags, void *data) {
  int rv = 0;
  printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
  return rv;
}

// Makes a symlink between the two given locations
int nufs_symlink(const char *to, const char *from) {
  int rv = storage_mknod(from, 0120000);
  if (rv < 0) {
    return rv;
  }
  storage_write(from, to, strlen(to), 0);
  return 0;
}

// Reads a link
int nufs_readlink(const char *path, char *buf, size_t size) {
  return storage_read(path, buf, size, 0);
}

void nufs_init_ops(struct fuse_operations *ops) {
  memset(ops, 0, sizeof(struct fuse_operations));
  ops->access = nufs_access;
  ops->getattr = nufs_getattr;
  ops->readdir = nufs_readdir;
  ops->mknod = nufs_mknod;
  ops->mkdir = nufs_mkdir;
  ops->link = nufs_link;
  ops->unlink = nufs_unlink;
  ops->rmdir = nufs_rmdir;
  ops->rename = nufs_rename;
  ops->chmod = nufs_chmod;
  ops->truncate = nufs_truncate;
  ops->open = nufs_open;
  ops->read = nufs_read;
  ops->write = nufs_write;
  ops->utimens = nufs_utimens;
  ops->ioctl = nufs_ioctl;
  ops->readlink = nufs_readlink;
  ops->symlink = nufs_symlink;
};

struct fuse_operations nufs_ops;

int main(int argc, char *argv[]) {
  assert(argc > 2 && argc < 6);
  storage_init(argv[--argc]);
  nufs_init_ops(&nufs_ops);
  return fuse_main(argc, argv, &nufs_ops, NULL);
}
