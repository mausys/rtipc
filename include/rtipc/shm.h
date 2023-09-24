#pragma once

#include <stddef.h>
#include <stdbool.h>

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


size_t ri_shm_calc_size(const size_t c2s_chns[], const size_t s2c_chns[]);

int ri_shm_map_channels(const ri_shm_t *shm, const size_t c2s_chns[], const size_t s2c_chns[]);

int ri_shm_get_rx_channel(const ri_shm_t *shm, unsigned idx, ri_chn_dir_t dir, ri_rchn_t *chn);

int ri_shm_get_tx_channel(const ri_shm_t *shm, unsigned idx, ri_chn_dir_t dir, ri_tchn_t *chn);


#ifdef __cplusplus
}
#endif
