// Disk storage abstracttion.
//
// Feel free to use as inspiration. Provided as-is.

// based on cs3650 starter code

#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "slist.h"

extern const int BLOCK_COUNT;
extern const int BLOCK_SIZE;
extern const int NUFS_SIZE;
extern const int BLOCK_BITMAP_SIZE;

void storage_init(const char *path);
int storage_stat(const char *path, struct stat *st);
int storage_read(const char *path, char *buf, size_t size, off_t offset);
int storage_write(const char *path, const char *buf, size_t size, off_t offset);
int storage_truncate(const char *path, off_t size);
void storage_find_parent(const char *fullpath, char *dir);
int storage_mknod(const char *path, int mode);
int storage_unlink(const char *path);
int storage_link(const char *from, const char *to);
int storage_rename(const char *from, const char *to);
int storage_set_time(const char *path, const struct timespec ts[2]);
int storage_can_find(const char *path);

#endif
