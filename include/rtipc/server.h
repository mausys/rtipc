#pragma once

#include <sys/types.h>

#include <rtipc/shm.h>
#include <rtipc/om.h>


#ifdef __cplusplus
extern "C" {
#endif

ri_shm_t* ri_server_create_anon_shm(const ri_obj_desc_t *c2s_chns[], const ri_obj_desc_t *s2c_chns[]);

ri_shm_t* ri_server_create_named_shm(const ri_obj_desc_t *c2s_chns[], const ri_obj_desc_t *s2c_chns[], const char *name, mode_t mode);

int ri_server_get_consumer(const ri_shm_t *shm, unsigned idx, ri_consumer_t *cns);

int ri_server_get_producer(const ri_shm_t *shm, unsigned idx, ri_producer_t *prd);

#ifdef __cplusplus
}
#endif
