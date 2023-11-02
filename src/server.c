#include "rtipc/server.h"


static ri_shm_t* create_shm_for_channels(const size_t c2s_chns[], const size_t s2c_chns[], const char *name, mode_t mode)
{
    ri_shm_t *shm = NULL;

    size_t shm_size = ri_calc_shm_size(c2s_chns, s2c_chns);

    if (name)
        shm = ri_named_shm_new(shm_size, name, mode);
    else
        shm = ri_anon_shm_new(shm_size);
    if (!shm)
        return NULL;

    int r = ri_shm_map_channels(shm, c2s_chns, s2c_chns);

    if (r < 0) {
        ri_shm_delete(shm);
        return NULL;
    }

    return shm;
}


static ri_shm_t* create_shm_for_objects(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[], const char *name, mode_t mode)
{
    unsigned n_c2s = 0;

    if (c2s_objs) {
        for (const ri_object_t **d = c2s_objs; *d; d++)
            n_c2s++;
    }

    size_t c2s_sizes[n_c2s + 1];

    for (unsigned i = 0; i < n_c2s; i++)
        c2s_sizes[i] = ri_calc_buffer_size(c2s_objs[i]);

    c2s_sizes[n_c2s] = 0;

    unsigned n_s2c = 0;

    if (s2c_objs) {
        for (const ri_object_t **d = s2c_objs; *d; d++)
            n_s2c++;
    }

    size_t s2c_sizes[n_s2c + 1];

    for (unsigned i = 0; i < n_s2c; i++)
        s2c_sizes[i] = ri_calc_buffer_size(s2c_objs[i]);

    s2c_sizes[n_s2c] = 0;

    return create_shm_for_channels(c2s_sizes, s2c_sizes, name, mode);
}

int ri_server_get_consumer(const ri_shm_t *shm, unsigned idx, ri_consumer_t *cns)
{
    return ri_shm_get_consumer(shm, idx, RI_CHN_C2S, cns);
}


int ri_server_get_producer(const ri_shm_t *shm, unsigned idx, ri_producer_t *prd)
{
    return ri_shm_get_producer(shm, idx, RI_CHN_S2C, prd);
}


ri_shm_t* ri_create_anon_shm_for_channels(const size_t c2s_chns[], const size_t s2c_chns[])
{
    return create_shm_for_channels(c2s_chns, s2c_chns, NULL, 0);
}


ri_shm_t* ri_create_named_shm_for_channels(const size_t c2s_chns[], const size_t s2c_chns[], const char *name, mode_t mode)
{
    if (!name)
        return NULL;

    return create_shm_for_channels(c2s_chns, s2c_chns, name, mode);
}


ri_shm_t* ri_create_anon_shm_for_objects(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[])
{
    return create_shm_for_objects(c2s_objs, s2c_objs, NULL, 0);
}


ri_shm_t* ri_create_named_shm_for_objects(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[], const char *name, mode_t mode)
{
    if (!name)
        return NULL;

    return create_shm_for_objects(c2s_objs, s2c_objs, name, mode);
}
