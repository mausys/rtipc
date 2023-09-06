#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct shm {
    void *base;
    int fd;
    size_t size;
} shm_t;

int shm_create(shm_t *shm, size_t size);
int shm_map(shm_t *shm, size_t size, int fd);
void shm_destroy(shm_t *shm);

#ifdef __cplusplus
}
#endif
