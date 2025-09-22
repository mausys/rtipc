#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdatomic.h>


typedef struct ri_shm ri_shm_t;

ri_shm_t* ri_shm_new(size_t size);
ri_shm_t* ri_shm_map(int fd);


void ri_shm_ref(ri_shm_t *shm);
void ri_shm_unref(ri_shm_t *shm);

void *ri_shm_ptr(const ri_shm_t *shm, size_t offset);

size_t ri_shm_size(const ri_shm_t *shm);

int ri_shm_fd(const ri_shm_t *shm);
