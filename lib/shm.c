#define _GNU_SOURCE

#include "shm.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>


#include <sys/mman.h> // memfd_create
#include <sys/stat.h> // fstat

#include "log.h"


static int shm_init(ri_shm_t *shm, bool sealing)
{
    int r = ftruncate(shm->fd, shm->size);

    if (r < 0) {
        LOG_ERR("ftruncate to size=%zu failed: %s", shm->size, strerror(errno));
        return r;
    }

    if (sealing) {
        r = fcntl(shm->fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL);

        if (r < 0) {
            LOG_ERR("fcntl F_ADD_SEALS failed: %s", strerror(errno));
            return r;
        }
    }

    shm->mem = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);

    if (shm->mem == MAP_FAILED) {
        LOG_ERR("mmap for %d with size=%zu failed: %s", shm->fd, shm->size, strerror(errno));
        return r;
    }

    LOG_INF("mmaped shared memory size=%zu, fd=%d on %p", shm->size, shm->fd, shm->mem);

    return 0;
}


ri_shm_t* ri_shm_anon_new(size_t size)
{
    static atomic_uint anr = 0;
    ri_shm_t *shm = malloc(sizeof(ri_shm_t));

    if (!shm)
        return NULL;

    unsigned nr = atomic_fetch_add_explicit(&anr, 1, memory_order_relaxed);
    char name[64];

    snprintf(name, sizeof(name) - 1, "rtipc_%u", nr);

    int r = memfd_create(name, MFD_ALLOW_SEALING);

    if (r < 0) {
        LOG_ERR("memfd_create failed for %s: %s", name, strerror(errno));
        goto fail_create;
    }

    *shm = (ri_shm_t) {
        .size = size,
        .fd = r,
        .owner = true,
    };

    r = shm_init(shm, true);

    if (r < 0)
        goto fail_init;

    LOG_INF("mmaped shared memory size=%zu, fd=%d on %p", shm->size, shm->fd, shm->mem);

    return shm;

fail_init:
    close(shm->fd);
fail_create:
    free(shm);
    return NULL;
}


ri_shm_t* ri_shm_named_new(size_t size, const char *name, mode_t mode)
{
    ri_shm_t *shm = malloc(sizeof(ri_shm_t));

    if (!shm)
        return NULL;

    *shm = (ri_shm_t) {
        .size = size,
        .owner = true,
    };

    shm->path = strdup(name);

    if (!shm->path)
        goto fail_path;

    int r = shm_open(shm->path, O_CREAT | O_EXCL | O_RDWR, mode);

    if (r < 0) {
        LOG_ERR("shm_open failed for %s: %s", name, strerror(errno));
        goto fail_create;
    }

    shm->fd = r;

    r = shm_init(shm, false);

    if (r < 0)
        goto fail_init;

    LOG_INF("create shared memory name=%s size=%zu, fd=%d on %p", name, shm->size, shm->fd, shm->mem);

    return shm;

fail_init:
    close(shm->fd);
    shm_unlink(shm->path);
fail_create:
    free(shm->path);
fail_path:
    free(shm);
    return NULL;
}


ri_shm_t* ri_shm_map(int fd)
{
    struct stat stat;
    ri_shm_t *shm = malloc(sizeof(ri_shm_t));

    if (!shm)
        return NULL;

    *shm = (ri_shm_t) {
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

    LOG_INF("maped shared memory name=%s size=%zu, on %p", shm->path, shm->size, shm->mem);

    return shm;

fail_map:
fail_stat:
    free(shm);
    return NULL;
}


ri_shm_t* ri_shm_named_map(const char *name)
{
    struct stat stat;
    ri_shm_t *shm = calloc(1, sizeof(ri_shm_t));

    if (!shm)
        return NULL;

    shm->path = strdup(name);

    if (!shm->path)
        goto fail_path;

    int r = shm_open(shm->path, O_EXCL | O_RDWR, 0);

    if (r < 0) {
        LOG_ERR("shm_open for %s failed: %s", shm->path, strerror(errno));
        goto fail_open;
    }

    shm->fd = r;

    r = fstat(shm->fd, &stat);

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

    LOG_INF("maped shared memory name=%s size=%zu, on %p", shm->path, shm->size, shm->mem);

    return shm;

fail_map:
fail_stat:
    close(shm->fd);
fail_open:
    free(shm->path);
fail_path:
    free(shm);
    return NULL;
}


void ri_shm_delete(ri_shm_t *shm)
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
