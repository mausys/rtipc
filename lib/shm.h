#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct ri_shm
{
  void *mem;
  size_t size;
  int fd;
  char *path;
  bool owner;
} ri_shm_t;

ri_shm_t* ri_shm_anon_new(size_t size);

ri_shm_t* ri_shm_named_new(size_t size, const char *name, mode_t mode);

ri_shm_t* ri_shm_new(int fd);

ri_shm_t* ri_shm_named_map(const char *name);

void ri_shm_delete(ri_shm_t *shm);

int ri_shm_get_fd(const ri_shm_t *shm);
