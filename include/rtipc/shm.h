#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#include <rtipc/channel.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    RI_CHN_C2S = 0,
    RI_CHN_S2C = 1
} ri_chn_dir_t;


typedef struct ri_shm {
    void *p;
    size_t size;
} ri_shm_t;


ri_shm_t* ri_anon_shm_new(size_t size);

ri_shm_t* ri_named_shm_new(size_t size, const char *name, mode_t mode);

ri_shm_t* ri_map_shm(int fd);

ri_shm_t* ri_map_named_shm(const char *name);

size_t ri_calc_shm_size(const size_t c2s_chn_sizes[], const size_t s2c_chn_sizes[]);

int ri_shm_map_channels(const ri_shm_t *shm, const size_t c2s_chn_sizes[], const size_t s2c_chn_sizes[]);

int ri_shm_get_rx_channel(const ri_shm_t *shm, unsigned idx, ri_chn_dir_t dir, ri_rchn_t *chn);

int ri_shm_get_tx_channel(const ri_shm_t *shm, unsigned idx, ri_chn_dir_t dir, ri_tchn_t *chn);

void ri_shm_delete(ri_shm_t *shm);

int ri_shm_get_fd(const ri_shm_t* shm);

#ifdef __cplusplus
}
#endif
