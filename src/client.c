#include "rtipc/client.h"


int ri_client_get_rx_channel(const ri_shm_t *shm, unsigned idx, ri_rchn_t *chn)
{
    return ri_shm_get_rx_channel(shm, idx, RI_CHN_S2C, chn);
}


int ri_client_get_tx_channel(const ri_shm_t *shm, unsigned idx, ri_tchn_t *chn)
{
    return ri_shm_get_tx_channel(shm, idx, RI_CHN_C2S, chn);
}
