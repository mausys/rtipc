#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct ri_shm {
    void *mem;
    size_t size;
    int fd;
    char *path;
    bool owner;
} ri_shm_t;


ri_shm_t* ri_sys_anon_new(size_t size);

ri_shm_t* ri_sys_named_new(size_t size, const char *name, mode_t mode);

ri_shm_t* ri_sys_map(int fd);

ri_shm_t* ri_sys_map_named(const char *name);

void ri_shm_delete(ri_shm_t *shm);

int ri_shm_get_fd(const ri_shm_t* shm);
