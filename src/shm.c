#include "shm.h"

#define _GNU_SOURCE

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>


#include <sys/mman.h> // memfd_create


#include "log.h"


struct shm {
    void *addr;
    int fd;
    size_t size;
};

static int create_shm(size_t size)
{
    static atomic_uint anr = 0;
    unsigned nr = atomic_fetch_add_explicit(&anr, 1, memory_order_relaxed);
    int r, fd;

    char name[64];

    snprintf(name, sizeof(name) - 1, "rtipc_%u", nr);

    r = memfd_create(name, MFD_ALLOW_SEALING);

    if (r < 0) {
        LOG_ERR("memfd_create failed for %s: %s", name, strerror(errno));
        return -1;
    }

    fd = r;

    r = ftruncate(fd, size);

    if (r < 0) {
        LOG_ERR("ftruncate to size=%zu failed for %s: %s", size, name, strerror(errno));
        goto fail;
    }

    r = fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL);

    if (r < 0) {
        LOG_ERR("fcntl F_ADD_SEALS failed for %s: %s", name, strerror(errno));
        goto fail;
    }

    LOG_INF("created shared memory name=%s, size=%zu, fd=%d", name, size, fd);
    return fd;

fail:
    close(fd);
    return -1;
}


shm_t* shm_map(size_t size, int fd)
{
    shm_t *shm = malloc(sizeof(shm_t));

    if (!shm)
        return shm;

    *shm = (shm_t) {
        .size = size,
        .fd = fd,
    };

    shm->addr = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);

    if (shm->addr == MAP_FAILED) {
        LOG_ERR("mmap for %d with size=%zu failed: %s", shm->fd, shm->size, strerror(errno));
        goto fail;
    }

    LOG_INF("mmaped shared memory size=%zu, fd=%d on %p", shm->size, shm->fd, shm->addr);

    return shm;

fail:
    free(shm);
    return NULL;
}


shm_t* shm_create(size_t size)
{
    shm_t *shm = malloc(sizeof(shm_t));

    if (!shm)
        return shm;

    *shm = (shm_t) {
        .size = size,
    };

    shm->fd = create_shm(size);

    if (shm->fd < 0)
        goto fail_create;

    shm->addr = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);

    if (shm->addr == MAP_FAILED) {
        LOG_ERR("mmap for %d with size=%zu failed: %s", shm->fd, shm->size, strerror(errno));
        goto fail_mmap;
    }

    LOG_INF("mmaped shared memory size=%zu, fd=%d on %p", shm->size, shm->fd, shm->addr);

    return shm;

fail_mmap:
    close(shm->fd);
fail_create:
    free(shm);
    return NULL;
}


void shm_delete(shm_t *shm)
{
    munmap(shm->addr, shm->size);
    close(shm->fd);
    free(shm);
}


void* shm_get_address(shm_t* shm)
{
    return shm->addr;
}


int shm_get_fd(shm_t* shm)
{
    return shm->fd;
}
