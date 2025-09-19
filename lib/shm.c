#define _GNU_SOURCE

#include "shm.h"

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h> // memfd_create
#include <sys/stat.h> // fstat

#include "log.h"

struct ri_shm
{
  atomic_int ref_cnt;
  void *mem;
  size_t size;
  int fd;
  char *path;
  bool owner;
};

static int shm_init(int fd, size_t size, bool sealing)
{
  int r = ftruncate(fd, size);

  if (r < 0) {
    LOG_ERR("ftruncate to size=%zu failed: %s", size, strerror(errno));
    return r;
  }

  if (sealing) {
    r = fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL);

    if (r < 0) {
      LOG_ERR("fcntl F_ADD_SEALS failed: %s", strerror(errno));
      return r;
    }
  }

  return 0;
}

static void shm_delete(ri_shm_t *shm)
{
  munmap(shm->mem, shm->size);
  close(shm->fd);
  if (shm->path) {
    if (shm->owner)
      shm_unlink(shm->path);
    free(shm->path);
  }
  free(shm);
}

ri_shm_t* ri_shm_new(size_t size)
{
  static atomic_uint anr = 0;

  unsigned nr = atomic_fetch_add_explicit(&anr, 1, memory_order_relaxed);
  char name[64];

  snprintf(name, sizeof(name) - 1, "rtipc_%u", nr);

  int fd = memfd_create(name, MFD_ALLOW_SEALING);

  if (fd < 0) {
    LOG_ERR("memfd_create failed for %s: %s", name, strerror(errno));
    goto fail_create;
  }

  int r = shm_init(fd, size, true);

  if (r < 0)
    goto fail_init;

  ri_shm_t *shm = ri_shm_new(fd);

  if (!shm)
    goto fail_map;

  shm->owner = true;

  LOG_INF("mmapped shared memory size=%zu, fd=%d on %p", shm->size, shm->fd, shm->mem);

  return shm;

fail_init:
fail_map:
  close(fd);
fail_create:
  return NULL;
}


ri_shm_t* ri_shm_map(int fd)
{
  struct stat stat;
  ri_shm_t *shm = malloc(sizeof(ri_shm_t));

  if (!shm)
    return NULL;

  *shm = (ri_shm_t) {
    .ref_cnt = 1,
    .fd = fd,
  };

  int r = fstat(shm->fd, &stat);

  if (r < 0) {
    LOG_ERR("fstat for %s failed: %s", shm->path, strerror(errno));
    goto fail_stat;
  }

  shm->size = stat.st_size;

  shm->mem = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);

  if (shm->mem == MAP_FAILED) {
    LOG_ERR("mmap for %s with size=%zu failed: %s", shm->path, shm->size, strerror(errno));
    goto fail_map;
  }

  LOG_INF("mapped shared memory name=%s size=%zu, on %p", shm->path, shm->size, shm->mem);

  return shm;

fail_map:
fail_stat:
  free(shm);
  return NULL;
}


void ri_shm_ref(ri_shm_t *shm)
{
  atomic_fetch_add(&shm->ref_cnt, 1);
}

void ri_shm_unref(ri_shm_t *shm)
{
  if (atomic_fetch_sub(&shm->ref_cnt, 1) == 1) {
    shm_delete(shm);
  }
}


void *ri_shm_ptr(const ri_shm_t *shm, size_t offset)
{
  if (offset >= shm->size)
    return NULL;

  return mem_offset(shm->mem, offset);
}


size_t ri_shm_size(const ri_shm_t *shm)
{
  return shm->size;
}

