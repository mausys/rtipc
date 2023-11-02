#pragma once

#include <sys/types.h>

#include <rtipc/shm.h>
#include <rtipc/object.h>


#ifdef __cplusplus
extern "C" {
#endif

ri_shm_t* ri_create_anon_shm_for_channels(const size_t c2s_chns[], const size_t s2c_chns[]);

ri_shm_t* ri_create_named_shm_for_channels(const size_t c2s_chns[], const size_t s2c_chns[], const char *name, mode_t mode);

ri_shm_t* ri_create_anon_shm_for_objects(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[]);

ri_shm_t* ri_create_named_shm_for_objects(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[], const char *name, mode_t mode);

int ri_server_get_consumer(const ri_shm_t *shm, unsigned idx, ri_consumer_t *cns);

int ri_server_get_producer(const ri_shm_t *shm, unsigned idx, ri_producer_t *prd);

#ifdef __cplusplus
}
#endif
