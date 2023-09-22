#define _GNU_SOURCE

#include "rtipc/posix.h"

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

#include "rtipc/log.h"


#define ri_container_of(ptr, type, member) ({                   \
        const __typeof__( ((type *)0)->member ) *__mptr = (ptr);\
        (type *)( (char *)__mptr - (offsetof(type, member)));})

typedef struct ri_shm_posix {
    ri_shm_t shm;
    int fd;
} ri_shm_posix_t;


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


ri_shm_t* ri_posix_shm_map(int fd)
{
    struct stat stat;
    ri_shm_posix_t *shmp = malloc(sizeof(ri_shm_posix_t));

    if (!shmp)
        return NULL;


    int r = fstat(fd, &stat);

    if (r < 0) {
        LOG_ERR("fstat for %d failed: %s", fd, strerror(errno));
        goto fail;
    }

    *shmp = (ri_shm_posix_t) {
        .shm.size = stat.st_size,
        .fd = fd,
    };

    ri_shm_t *shm = &shmp->shm;

    shm->p = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shmp->fd, 0);

    if (shm->p == MAP_FAILED) {
        LOG_ERR("mmap for %d with size=%zu failed: %s", shmp->fd, shm->size, strerror(errno));
        goto fail;
    }

    LOG_INF("mmaped shared memory size=%zu, fd=%d on %p", shm->size, shmp->fd, shm->p);

    return shm;

fail:
    free(shmp);
    return NULL;
}


ri_shm_t* ri_posix_shm_create(size_t size)
{
    ri_shm_posix_t *shmp = malloc(sizeof(ri_shm_posix_t));

    if (!shmp)
        return NULL;

    *shmp = (ri_shm_posix_t) {
        .shm.size = size,
    };

    shmp->fd = create_shm(size);

    if (shmp->fd < 0)
        goto fail_create;

    ri_shm_t *shm = &shmp->shm;

    shm->p = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shmp->fd, 0);

    if (shm->p == MAP_FAILED) {
        LOG_ERR("mmap for %d with size=%zu failed: %s", shmp->fd, shm->size, strerror(errno));
        goto fail_mmap;
    }

    LOG_INF("mmaped shared memory size=%zu, fd=%d on %p", shm->size, shmp->fd, shm->p);

    return shm;

fail_mmap:
    close(shmp->fd);
fail_create:
    free(shmp);
    return NULL;
}


void ri_posix_shm_delete(ri_shm_t *shm)
{
    ri_shm_posix_t *shmp = ri_container_of(shm, ri_shm_posix_t, shm);
    munmap(shm->p, shm->size);
    close(shmp->fd);
    free(shmp);
}



int ri_posix_shm_get_fd(const ri_shm_t* shm)
{
    const ri_shm_posix_t *shmp = ri_container_of(shm, ri_shm_posix_t, shm);
    return shmp->fd;
}
