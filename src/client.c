#include "rtipc/client.h"


int ri_client_get_rx_channel(const ri_shm_t *shm, unsigned idx, ri_rchn_t *chn)
{
    return ri_shm_get_rx_channel(shm, idx, RI_CHN_S2C, chn);
}


int ri_client_get_tx_channel(const ri_shm_t *shm, unsigned idx, ri_tchn_t *chn)
{
    return ri_shm_get_tx_channel(shm, idx, RI_CHN_C2S, chn);
}


ri_shm_t* ri_client_map_shm(int fd)
{
    return ri_map_shm(fd);
}


ri_shm_t* ri_client_map_named_shm(const char *name)
{
    return ri_map_named_shm(name);
}
