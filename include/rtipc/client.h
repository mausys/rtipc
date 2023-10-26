#pragma once

#include <rtipc/shm.h>

#ifdef __cplusplus
extern "C" {
#endif

int ri_client_get_consumer(const ri_shm_t *shm, unsigned idx, ri_consumer_t *cns);

int ri_client_get_producer(const ri_shm_t *shm, unsigned idx, ri_producer_t *prd);

ri_shm_t* ri_client_map_shm(int fd);

ri_shm_t* ri_client_map_named_shm(const char *name);

#ifdef __cplusplus
}
#endif
