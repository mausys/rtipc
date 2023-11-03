#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "rtipc.h"
#include "log.h"
#include "mem_utils.h"

typedef struct {
    size_t offset;
    void **p;
} ri_object_map_t;


struct ri_producer_objects {
    ri_producer_t *prd;
    ri_object_map_t *objs;
    unsigned num;
    void *cache;
    void *buf;
    size_t buf_size;
};


struct ri_consumer_objects {
    ri_consumer_t *cns;
    ri_object_map_t *objs;
    unsigned num;
    void *buf;
    size_t buf_size;
};


static unsigned count_objs(const ri_object_t objs[])
{
    unsigned i;
    for (i = 0; objs[i].size != 0; i++)
        ;
    return i;
}


static void nullify_opjects(ri_object_map_t objs[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (objs[i].p)
            *objs[i].p = NULL;
    }
}


static void map_opjects(void *p, ri_object_map_t objs[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (objs[i].p)
            *objs[i].p = mem_offset(p, objs[i].offset);
    }
}


static ri_object_map_t* objs_new(const ri_object_t *objs, unsigned n)
{
    ri_object_map_t *maps = malloc(n * sizeof(ri_object_map_t));

    if (!maps)
        return NULL;

    size_t offset = 0;

    for (unsigned i = 0; i < n; i++) {
        if (objs[i].align != 0)
            offset = mem_align(offset, objs[i].align);

        maps[i] = (ri_object_map_t) {
            .offset = offset,
            .p = objs[i].p,
        };

        offset += objs[i].size;
    }

    return maps;
}


static ri_shm_t* objects_shm_new(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[], const char *name, mode_t mode)
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

    return name ? ri_named_shm_new(c2s_sizes, s2c_sizes, name, mode) : ri_anon_shm_new(c2s_sizes, s2c_sizes);
}


ri_shm_t* ri_objects_anon_shm_new(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[])
{
    return objects_shm_new(c2s_objs, s2c_objs, NULL, 0);
}


ri_shm_t* ri_objects_named_shm_new(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[], const char *name, mode_t mode)
{
    if (!name)
        return NULL;

    return objects_shm_new(c2s_objs, s2c_objs, name, mode);
}


size_t ri_calc_buffer_size(const ri_object_t objs[])
{
    size_t size = 0;

    for (unsigned i = 0; objs[i].size != 0; i++) {
        if (objs[i].align != 0)
            size = mem_align(size, objs[i].align);

        size += objs[i].size;
    }

    return size;
}


ri_consumer_objects_t* ri_consumer_objects_new(ri_shm_t *shm, unsigned chn_id, const ri_object_t *objs)
{
    ri_consumer_objects_t *cos = malloc(sizeof(ri_consumer_objects_t));

    if (!cos)
        return NULL;

    ri_consumer_t *cns = ri_shm_get_consumer(shm, chn_id);

    if (!cns)
        return NULL;

    *cos = (ri_consumer_objects_t) {
        .cns = cns,
        .buf_size = ri_calc_buffer_size(objs),
        .num = count_objs(objs),
    };

    size_t chn_size = ri_consumer_get_buffer_size(cns);

    if (cos->buf_size > chn_size) {
        LOG_ERR("objects size exeeds channel size; objects size=%zu, channel size=%zu", cos->buf_size, chn_size);
        goto fail;
    }

    cos->objs = objs_new(objs, cos->num);

    if (!cos->objs)
        goto fail;

    return cos;

fail:
    free(cos);
    return NULL;
}


void ri_consumer_objects_delete(ri_consumer_objects_t* cos)
{
    nullify_opjects(cos->objs, cos->num);
    free(cos->objs);
    free(cos);
}


ri_producer_objects_t* ri_producer_objects_new(ri_shm_t *shm, unsigned chn_id, const ri_object_t *objs, bool cache)
{
    ri_producer_objects_t *pos = malloc(sizeof(ri_producer_objects_t));

    if (!pos)
        return NULL;

    ri_producer_t *prd = ri_shm_get_producer(shm, chn_id);

    if (!prd)
        return NULL;

    *pos = (ri_producer_objects_t) {
        .prd = prd,
        .num = count_objs(objs),
        .buf_size = ri_calc_buffer_size(objs),
    };

    size_t chn_size = ri_producer_get_buffer_size(prd);

    if (pos->buf_size > chn_size) {
        LOG_ERR("objects size exeeds channel size; objects size=%zu, channel size=%zu", pos->buf_size, chn_size);
        goto fail;
    }

    pos->objs = objs_new(objs, pos->num);

    pos->buf = ri_producer_swap(pos->prd);

    if (cache) {
        pos->cache = calloc(1, pos->buf_size);

        if (!pos->cache)
            goto fail;

        map_opjects(pos->cache, pos->objs, pos->num);
    } else {
        map_opjects(pos->buf, pos->objs, pos->num);
    }

    if (!pos->objs)
        goto fail;

    return pos;

fail:
    if (pos->cache)
        free(pos->cache);
    free(pos);
    return NULL;
}


void ri_producer_objects_delete(ri_producer_objects_t* pos)
{
    nullify_opjects(pos->objs, pos->num);

    if (pos->cache)
        free(pos->cache);

    free(pos->objs);
    free(pos);
}


void ri_producer_objects_update(ri_producer_objects_t *pos)
{
    if (pos->cache) {
        memcpy(pos->buf, pos->cache, pos->buf_size);
        pos->buf = ri_producer_swap(pos->prd);
    } else {
        pos->buf = ri_producer_swap(pos->prd);
        map_opjects(pos->buf, pos->objs, pos->num);
    }
}


bool ri_producer_objects_ackd(const ri_producer_objects_t *pos)
{
    return ri_producer_ackd(pos->prd);
}


int ri_consumer_objects_update(ri_consumer_objects_t *cos)
{
    void *old = cos->buf;

    cos->buf = ri_consumer_fetch(cos->cns);

    if (!cos->buf) {
        nullify_opjects(cos->objs, cos->num);
        return -1;
    }

    if (old == cos->buf)
        return 0;

    map_opjects(cos->buf, cos->objs, cos->num);

    return 1;
}

