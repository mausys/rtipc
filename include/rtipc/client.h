#pragma once

#include <rtipc/shm.h>

#ifdef __cplusplus
extern "C" {
#endif

int ri_client_get_rx_channel(const ri_shm_t *shm, unsigned idx, ri_rchn_t *chn);

int ri_client_get_tx_channel(const ri_shm_t *shm, unsigned idx, ri_tchn_t *chn);

ri_shm_t* ri_client_map_shm(int fd);

ri_shm_t* ri_client_map_named_shm(const char *name);

#ifdef __cplusplus
}
#endif
