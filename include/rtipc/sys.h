#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include <rtipc/shm.h>

#ifdef __cplusplus
extern "C" {
#endif


ri_shm_t* ri_anon_shm_create(size_t size);
ri_shm_t* ri_named_shm_create(size_t size, const char *path, mode_t mode);
ri_shm_t* ri_shm_map(int fd);
ri_shm_t* ri_named_shm_map(const char *path);
void ri_shm_delete(ri_shm_t *shm);
int ri_shm_get_fd(const ri_shm_t* shm);

#ifdef __cplusplus
}
#endif
