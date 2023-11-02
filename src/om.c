#include "rtipc/om.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "rtipc/log.h"

#include "mem_utils.h"

typedef struct {
    size_t offset;
    void **p;
} ri_object_map_t;


struct ri_producer_objects {
    ri_producer_t prd;
    ri_object_map_t *objs;
    unsigned num;
    void *cache;
    void *buf;
    size_t buf_size;
};


struct ri_consumer_objects {
    ri_consumer_t cns;
    ri_object_map_t *objs;
    unsigned num;
    void *buf;
    size_t buf_size;
};


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


static unsigned count_objs(const ri_object_t objs[])
{
    unsigned i;
    for (i = 0; objs[i].size != 0; i++)
        ;
    return i;
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


ri_consumer_objects_t* ri_consumer_objects_new(const ri_consumer_t *cns, const ri_object_t *objs)
{
    ri_consumer_objects_t *cos = malloc(sizeof(ri_consumer_objects_t));

    if (!cos)
        return NULL;

    *cos = (ri_consumer_objects_t) {
        .cns = *cns,
        .buf_size = ri_calc_buffer_size(objs),
        .num = count_objs(objs),
    };

    size_t chn_size = ri_channel_get_buffer_size(&cns->chn);

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


ri_producer_objects_t* ri_producer_objects_new(const ri_producer_t *prd, const ri_object_t *objs, bool cache)
{
    ri_producer_objects_t *pos = malloc(sizeof(ri_producer_objects_t));

    if (!pos)
        return NULL;

    *pos = (ri_producer_objects_t) {
        .prd = *prd,
        .num = count_objs(objs),
        .buf_size = ri_calc_buffer_size(objs),
        .buf = prd->chn.bufs[prd->current],
    };

    size_t chn_size = ri_channel_get_buffer_size(&prd->chn);

    if (pos->buf_size > chn_size) {
        LOG_ERR("objects size exeeds channel size; objects size=%zu, channel size=%zu", pos->buf_size, chn_size);
        goto fail;
    }

    pos->objs = objs_new(objs, pos->num);

    pos->buf = ri_producer_swap(&pos->prd);

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
        pos->buf = ri_producer_swap(&pos->prd);
    } else {
        pos->buf = ri_producer_swap(&pos->prd);
        map_opjects(pos->buf, pos->objs, pos->num);
    }
}


bool ri_producer_objects_ackd(const ri_producer_objects_t *pos)
{
    return ri_producer_ackd(&pos->prd);
}


int ri_consumer_objects_update(ri_consumer_objects_t *cos)
{
    void *old = cos->buf;

    cos->buf = ri_consumer_fetch(&cos->cns);

    if (!cos->buf) {
        nullify_opjects(cos->objs, cos->num);
        return -1;
    }

    if (old == cos->buf)
        return 0;

    map_opjects(cos->buf, cos->objs, cos->num);

    return 1;
}


