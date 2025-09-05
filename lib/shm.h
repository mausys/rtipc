#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdatomic.h>

typedef struct ri_shm
{
  atomic_int ref_cnt;
  void *mem;
  size_t size;
  int fd;
  char *path;
  bool owner;
} ri_shm_t;



ri_shm_t* ri_shm_anon_new(size_t size);

ri_shm_t* ri_shm_named_new(size_t size, const char *name, mode_t mode);

ri_shm_t* ri_shm_new(int fd);

void ri_shm_ref(ri_shm_t *shm);
void ri_shm_unref(ri_shm_t *shm);

ri_shm_t* ri_shm_named_map(const char *name);

void ri_shm_delete(ri_shm_t *shm);

int ri_shm_get_fd(const ri_shm_t *shm);
