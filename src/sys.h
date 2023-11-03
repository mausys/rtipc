#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#include "types.h"

ri_shm_t* ri_sys_anon_shm_new(size_t size);

ri_shm_t* ri_sys_named_shm_new(size_t size, const char *name, mode_t mode);

ri_shm_t* ri_sys_map_shm(int fd);

ri_shm_t* ri_sys_map_named_shm(const char *name);

void ri_sys_shm_delete(ri_shm_t *shm);

int ri_sys_shm_get_fd(const ri_shm_t* shm);
