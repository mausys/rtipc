#include "rtipc/client.h"


int ri_client_get_consumer(const ri_shm_t *shm, unsigned idx, ri_consumer_t *cns)
{
    return ri_shm_get_consumer(shm, idx, RI_CHN_S2C, cns);
}


int ri_client_get_producer(const ri_shm_t *shm, unsigned idx, ri_producer_t *prd)
{
    return ri_shm_get_producer(shm, idx, RI_CHN_C2S, prd);
}


ri_shm_t* ri_client_map_shm(int fd)
{
    return ri_map_shm(fd);
}


ri_shm_t* ri_client_map_named_shm(const char *name)
{
    return ri_map_named_shm(name);
}
