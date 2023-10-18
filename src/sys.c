#define _GNU_SOURCE

#include "rtipc/shm.h"

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

typedef struct ri_sys {
    ri_shm_t shm;
    int fd;
    char *path;
    bool owner;
} ri_sys_t;


static int sys_init(ri_sys_t *sys, bool sealing)
{
    int r = ftruncate(sys->fd, sys->shm.size);

    if (r < 0) {
        LOG_ERR("ftruncate to size=%zu failed: %s", sys->shm.size, strerror(errno));
        return r;
    }

    if (sealing) {
        r = fcntl(sys->fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL);

        if (r < 0) {
            LOG_ERR("fcntl F_ADD_SEALS failed: %s", strerror(errno));
            return r;
        }
    }

    sys->shm.p = mmap(NULL, sys->shm.size, PROT_READ | PROT_WRITE, MAP_SHARED, sys->fd, 0);

    if (sys->shm.p == MAP_FAILED) {
        LOG_ERR("mmap for %d with size=%zu failed: %s", sys->fd, sys->shm.size, strerror(errno));
        return r;
    }

    LOG_INF("mmaped shared memory size=%zu, fd=%d on %p", sys->shm.size, sys->fd, sys->shm.p);

    return 0;
}


ri_shm_t* ri_anon_shm_new(size_t size)
{
    static atomic_uint anr = 0;
    ri_sys_t *sys = malloc(sizeof(ri_sys_t));

    if (!sys)
        return NULL;

    unsigned nr = atomic_fetch_add_explicit(&anr, 1, memory_order_relaxed);
    char name[64];

    snprintf(name, sizeof(name) - 1, "rtipc_%u", nr);

    int r = memfd_create(name, MFD_ALLOW_SEALING);

    if (r < 0) {
        LOG_ERR("memfd_create failed for %s: %s", name, strerror(errno));
        goto fail_create;
    }

    *sys = (ri_sys_t) {
        .shm.size = size,
        .fd = r,
        .owner = true,
    };

    r = sys_init(sys, true);

    if (r < 0)
        goto fail_init;

    LOG_INF("mmaped shared memory size=%zu, fd=%d on %p", sys->shm.size, sys->fd, sys->shm.p);

    return &sys->shm;

fail_init:
    close(sys->fd);
fail_create:
    free(sys);
    return NULL;
}


ri_shm_t* ri_named_shm_new(size_t size, const char* name, mode_t mode)
{
    ri_sys_t *sys = malloc(sizeof(ri_sys_t));

    if (!sys)
        return NULL;

    *sys = (ri_sys_t) {
        .shm.size = size,
        .owner = true,
    };

    sys->path = strdup(name);

    if (!sys->path)
        goto fail_path;

    int r = shm_open(sys->path, O_CREAT | O_EXCL | O_RDWR, mode);

    if (r < 0) {
        LOG_ERR("shm_open failed for %s: %s", name, strerror(errno));
        goto fail_create;
    }

    sys->fd = r;

    r = sys_init(sys, false);

    if (r < 0)
        goto fail_init;

    LOG_INF("create shared memory name=%s size=%zu, fd=%d on %p", name, sys->shm.size, sys->fd, sys->shm.p);

    return &sys->shm;

fail_init:
    close(sys->fd);
    shm_unlink(sys->path);
fail_create:
    free(sys->path);
fail_path:
    free(sys);
    return NULL;
}


ri_shm_t* ri_map_shm(int fd)
{
    struct stat stat;
    ri_sys_t *sys = malloc(sizeof(ri_sys_t));

    if (!sys)
        return NULL;

    *sys = (ri_sys_t) {
       .fd = fd,
       .owner = false,
    };

    int r = fstat(sys->fd, &stat);

    if (r < 0) {
        LOG_ERR("fstat for %s failed: %s", sys->path, strerror(errno));
        goto fail_stat;
    }

    sys->shm.size = stat.st_size;

    sys->shm.p = mmap(NULL, sys->shm.size, PROT_READ | PROT_WRITE, MAP_SHARED, sys->fd, 0);

    if (sys->shm.p == MAP_FAILED) {
        LOG_ERR("mmap for %s with size=%zu failed: %s", sys->path, sys->shm.size, strerror(errno));
        goto fail_map;
    }

    LOG_INF("maped shared memory name=%s size=%zu, on %p", sys->path, sys->shm.size, sys->shm.p);

    return &sys->shm;

fail_map:
fail_stat:
    free(sys);
    return NULL;
}


ri_shm_t* ri_map_named_shm(const char *name)
{
    struct stat stat;
    ri_sys_t *sys = calloc(1, sizeof(ri_sys_t));

    if (!sys)
        return NULL;

    sys->path = strdup(name);

    if (!sys->path)
        goto fail_path;

    int r = shm_open(sys->path, O_EXCL | O_RDWR, 0);

    if (r < 0) {
        LOG_ERR("shm_open for %s failed: %s", sys->path, strerror(errno));
        goto fail_open;
    }

    sys->fd = r;

    r = fstat(sys->fd, &stat);

    if (r < 0) {
        LOG_ERR("fstat for %s failed: %s", sys->path, strerror(errno));
        goto fail_stat;
    }

    sys->shm.size = stat.st_size;

    sys->shm.p = mmap(NULL, sys->shm.size, PROT_READ | PROT_WRITE, MAP_SHARED, sys->fd, 0);

    if (sys->shm.p == MAP_FAILED) {
        LOG_ERR("mmap for %s with size=%zu failed: %s", sys->path, sys->shm.size, strerror(errno));
        goto fail_map;
    }

    LOG_INF("maped shared memory name=%s size=%zu, on %p", sys->path, sys->shm.size, sys->shm.p);

    return &sys->shm;

fail_map:
fail_stat:
    close(sys->fd);
fail_open:
    free(sys->path);
fail_path:
    free(sys);
    return NULL;
}


void ri_shm_delete(ri_shm_t *shm)
{
    ri_sys_t *sys = ri_container_of(shm, ri_sys_t, shm);
    munmap(shm->p, shm->size);
    close(sys->fd);
    if (sys->path) {
        if (sys->owner)
            shm_unlink(sys->path);
        free(sys->path);
    }
    free(sys);
}



int ri_shm_get_fd(const ri_shm_t* shm)
{
    const ri_sys_t *sys = ri_container_of(shm, ri_sys_t, shm);
    return sys->fd;
}
