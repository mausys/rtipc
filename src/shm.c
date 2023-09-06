#include "shm.h"

#define _GNU_SOURCE

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


#include <sys/mman.h> // memfd_create


static int create_shm(size_t size)
{
    static atomic_uint anr = 0;
    unsigned nr = atomic_fetch_add_explicit(&anr, 1, memory_order_relaxed);
    int r, fd;

    char name[64];

    snprintf(name, sizeof(name) - 1, "rtipc_%u", nr);

    r = memfd_create(name, MFD_ALLOW_SEALING);

    if (r < 0)
        return -errno;

    fd = r;

    r = ftruncate(fd, size);

    if (r < 0) {
        r = -errno;
        goto fail;
    }

    r = fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL);

    if (r < 0) {
        r = -errno;
        goto fail;
    }

    return fd;

fail:
    close(fd);
    return r;
}


int shm_init(shm_t *shm, size_t size, int fd)
{
    shm->size = size;

    if (fd < 0) {
        shm->owner = true;
        shm->fd = create_shm(size);
        if (shm->fd < 0)
            return -errno;
    } else {
        shm->owner = false;
        shm->fd = fd;
    }

    shm->base = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);

    if (shm->base == MAP_FAILED) {
        if (shm->owner)
            close(shm->fd);
        return -errno;
    }

    return 0;
}


void shm_destroy(shm_t *shm)
{
    munmap(shm->base, shm->size);
    close(shm->fd);
}
