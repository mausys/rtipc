#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct shm shm_t;

shm_t* shm_create(size_t size);
shm_t* shm_map(size_t size, int fd);
void shm_delete(shm_t *shm);
void* shm_get_address(shm_t* shm);
int shm_get_fd(shm_t* shm);

#ifdef __cplusplus
}
#endif
