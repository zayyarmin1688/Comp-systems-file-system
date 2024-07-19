#ifndef RANDOMFUNCS_H
#define RANDOMFUNCS_H

#include <string.h>

static int streq(const char *aa, const char *bb) { return strcmp(aa, bb) == 0; }

static int min(int x, int y) { return (x < y) ? x : y; }

static int max(int x, int y) { return (x > y) ? x : y; }

static int clamp(int x, int v0, int v1) { return max(v0, min(x, v1)); }

static void join_to_path(char *buf, char *item) {
  int nn = strlen(buf);
  if (buf[nn - 1] != '/') {
    strcat(buf, "/");
  }
  strcat(buf, item);
}

#endif