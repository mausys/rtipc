#pragma once

#include <stddef.h>
#include <stdbool.h>

#include <rtipc/channel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ri_shm {
    void *p;
    size_t size;
} ri_shm_t;


size_t ri_shm_calc_chn_size(size_t buffer_size);

size_t ri_shm_set_rchn(const ri_shm_t *shm, size_t offset, size_t buf_size, bool last, ri_rchn_t *chn);

size_t ri_shm_set_tchn(const ri_shm_t *shm, size_t offset, size_t buf_size, bool last, ri_tchn_t *chn);

size_t ri_shm_get_rchn(const ri_shm_t *shm, size_t offset, ri_rchn_t *chn);

size_t ri_shm_get_tchn(const ri_shm_t *shm, size_t offset, ri_tchn_t *chn);

int ri_shm_count_rchn(const ri_shm_t *shm);

int ri_shm_count_tchn(const ri_shm_t *shm);

#ifdef __cplusplus
}
#endif
