#pragma once

#include <rtipc/shm.h>
#include <rtipc/object.h>


#ifdef __cplusplus
extern "C" {
#endif

ri_shm_t* ri_server_create_shm(const ri_obj_desc_t *c2s_chns[], const ri_obj_desc_t *s2c_chns[]);

int ri_server_get_rx_channel(const ri_shm_t *shm, unsigned idx, ri_rchn_t *chn);

int ri_server_get_tx_channel(const ri_shm_t *shm, unsigned idx, ri_tchn_t *chn);

#ifdef __cplusplus
}
#endif