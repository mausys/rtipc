#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <rtipc/shm.h>

#ifdef __cplusplus
extern "C" {
#endif


ri_shm_t* ri_posix_shm_create(size_t size);
ri_shm_t* ri_posix_shm_map(int fd);
void ri_posix_shm_delete(ri_shm_t *shm);
int ri_posix_shm_get_fd(const ri_shm_t* shm);

#ifdef __cplusplus
}
#endif
