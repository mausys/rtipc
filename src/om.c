#include "rtipc/om.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "rtipc/log.h"

#include "mem_utils.h"

typedef struct {
    size_t offset;
    size_t size;
    void **p;
} ri_obj_t;


struct ri_tom {
    ri_producer_t prd;
    ri_obj_t *objs;
    unsigned num;
    void *cache;
    void *buf;
    size_t buf_size;
};


struct ri_rom {
    ri_consumer_t cns;
    ri_obj_t *objs;
    unsigned num;
    void *buf;
    size_t buf_size;
};


static void nullify_opjects(ri_obj_t objs[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (objs[i].p)
            *objs[i].p = NULL;
    }
}


static void map_opjects(void *p, ri_obj_t objs[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (objs[i].p)
            *objs[i].p = mem_offset(p, objs[i].offset);
    }
}


static ri_obj_t* objs_new(const ri_obj_desc_t *descs, unsigned n)
{
    ri_obj_t *objs = malloc(n * sizeof(ri_obj_t));

    if (!objs)
        return NULL;

    size_t offset = 0;

    for (unsigned i = 0; i < n; i++) {
        if (descs[i].align != 0)
            offset = mem_align(offset, descs[i].align);

        objs[i] = (ri_obj_t) {
            .offset = offset,
            .size = descs[i].size,
            .p = descs[i].p,
        };

        offset += descs[i].size;
    }

    return objs;
}


static unsigned count_objs(const ri_obj_desc_t descs[])
{
    unsigned i;
    for (i = 0; descs[i].size != 0; i++)
        ;
    return i;
}


size_t ri_calc_buffer_size(const ri_obj_desc_t descs[])
{
    size_t size = 0;

    for (unsigned i = 0; descs[i].size != 0; i++) {
        if (descs[i].align != 0)
            size = mem_align(size, descs[i].align);

        size += descs[i].size;
    }

    return size;
}


ri_rom_t* ri_rom_new(const ri_consumer_t *cns, const ri_obj_desc_t *descs)
{
    ri_rom_t *rom = malloc(sizeof(ri_rom_t));

    if (!rom)
        return NULL;

    *rom = (ri_rom_t) {
        .cns = *cns,
        .buf_size = ri_calc_buffer_size(descs),
        .num = count_objs(descs),
    };

    size_t chn_size = ri_channel_get_buffer_size(&cns->chn);

    if (rom->buf_size > chn_size) {
        LOG_ERR("objects size exeeds channel size; objects size=%zu, channel size=%zu", rom->buf_size, chn_size);
        goto fail;
    }

    rom->objs = objs_new(descs, rom->num);

    if (!rom->objs)
        goto fail;

    return rom;

fail:
    free(rom);
    return NULL;
}


void ri_rom_delete(ri_rom_t* rom)
{
    nullify_opjects(rom->objs, rom->num);
    free(rom->objs);
    free(rom);
}


ri_tom_t* ri_tom_new(const ri_producer_t *prd, const ri_obj_desc_t *descs, bool cache)
{
    ri_tom_t *tom = malloc(sizeof(ri_tom_t));

    if (!tom)
        return NULL;

    *tom = (ri_tom_t) {
        .prd = *prd,
        .num = count_objs(descs),
        .buf_size = ri_calc_buffer_size(descs),
        .buf = prd->chn.bufs[prd->current],
    };

    size_t chn_size = ri_channel_get_buffer_size(&prd->chn);

    if (tom->buf_size > chn_size) {
        LOG_ERR("objects size exeeds channel size; objects size=%zu, channel size=%zu", tom->buf_size, chn_size);
        goto fail;
    }

    tom->objs = objs_new(descs, tom->num);

    tom->buf = ri_producer_swap(&tom->prd);

    if (cache) {
        tom->cache = calloc(1, tom->buf_size);

        if (!tom->cache)
            goto fail;

        map_opjects(tom->cache, tom->objs, tom->num);
    } else {
        map_opjects(tom->buf, tom->objs, tom->num);
    }

    if (!tom->objs)
        goto fail;

    return tom;

fail:
    if (tom->cache)
        free(tom->cache);
    free(tom);
    return NULL;
}


void ri_tom_delete(ri_tom_t* tom)
{
    nullify_opjects(tom->objs, tom->num);

    if (tom->cache)
        free(tom->cache);

    free(tom->objs);
    free(tom);
}


void ri_tom_update(ri_tom_t *tom)
{
    if (tom->cache) {
        memcpy(tom->buf, tom->cache, tom->buf_size);
        tom->buf = ri_producer_swap(&tom->prd);
    } else {
        tom->buf = ri_producer_swap(&tom->prd);
        map_opjects(tom->buf, tom->objs, tom->num);
    }
}


bool ri_tom_ackd(const ri_tom_t *tom)
{
    return ri_producer_ackd(&tom->prd);
}


int ri_rom_update(ri_rom_t *rom)
{
    void *old = rom->buf;

    rom->buf = ri_consumer_fetch(&rom->cns);

    if (!rom->buf) {
        nullify_opjects(rom->objs, rom->num);
        return -1;
    }

    if (old == rom->buf)
        return 0;

    map_opjects(rom->buf, rom->objs, rom->num);

    return 1;
}


