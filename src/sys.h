#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#include "types.h"

ri_sys_t* ri_sys_anon_new(size_t size);

ri_sys_t* ri_sys_named_new(size_t size, const char *name, mode_t mode);

ri_sys_t* ri_sys_map(int fd);

ri_sys_t* ri_sys_map_named(const char *name);

void ri_sys_delete(ri_sys_t *shm);

int ri_sys_get_fd(const ri_sys_t* shm);
