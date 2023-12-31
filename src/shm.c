#include "rtipc.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdalign.h>
#include <errno.h>

#include "log.h"
#include "mem_utils.h"
#include "sys.h"

#define MAGIC 0x1f0ca3be // lock-free zero-copy atomic triple buffer exchange :)

#define DIRECTION_MASK    0x1

#define LAST_CHANNEL_MASK 0x2


typedef struct {
    size_t offset;
    size_t buf_size;
    ri_xchg_t xchg;
} tbl_entry_t;


typedef struct {
    uint32_t n_cns; /**< number of consumers from server perspective */
    uint32_t n_prd; /**< number of producers from server perspective */
    size_t tbl_offset;
    size_t tbl_size;
    uint16_t align;
    uint8_t xchg_size;
    uint32_t magic;
} shm_hdr_t;


static unsigned count_channels(const size_t chns[])
{
    if (!chns)
        return 0;

    unsigned i;

    for (i = 0; chns[i] != 0; i++)
        ;

    return i;
}


static size_t calc_channel_size(size_t buf_size)
{
    return RI_NUM_BUFFERS * buf_size;
}


static size_t calc_shm_size(const size_t cns_sizes[], const size_t prd_sizes[])
{
    unsigned n_cns = count_channels(cns_sizes);
    unsigned n_prd = count_channels(prd_sizes);

    size_t offset = mem_align(sizeof(shm_hdr_t), alignof(tbl_entry_t));

    size_t tbl_size = (n_cns + n_prd) * sizeof(tbl_entry_t);
    offset = mem_align(offset + tbl_size, mem_alignment());

    for (unsigned i = 0; i < n_cns; i++) {
        size_t buf_size = mem_align(cns_sizes[i], mem_alignment());
        offset += calc_channel_size(buf_size);
    }

    for (unsigned i = 0; i < n_prd; i++) {
        size_t buf_size = mem_align(prd_sizes[i], mem_alignment());
        offset += calc_channel_size(buf_size);
    }

    return offset;
}

static bool check_hdr(const shm_hdr_t *hdr)
{
    if (hdr->magic != MAGIC) {
        LOG_ERR("invalid magic field: is=%x expected=%x", hdr->magic, MAGIC);
        return false;
    }

    if (hdr->align != (uint16_t)mem_alignment()) {
        LOG_ERR("invalid align field: is=%x expected=%x", hdr->align, (uint8_t)mem_alignment());
        return false;
    }

    if (hdr->xchg_size != (uint8_t)sizeof(ri_xchg_t)) {
        LOG_ERR("invalid xchg_size field: is=%x expected=%x", hdr->xchg_size, (uint8_t)sizeof(ri_xchg_t));
        return false;
    }

    return true;
}


static size_t map_channel(tbl_entry_t *chn, size_t offset, size_t buf_size, size_t shm_size)
{
    buf_size = mem_align(buf_size, mem_alignment());
    size_t chn_size = calc_channel_size(buf_size);

    if (offset + chn_size > shm_size) {
        LOG_ERR("channel(size=%zu) doesn't fit in shm(size=%zu)", chn_size, shm_size);
        return 0;
    }

    chn->buf_size = buf_size;
    chn->xchg = RI_BUFIDX_NONE;
    chn->offset = offset;

    return  offset + chn_size;
}


static void delete_channels(ri_shm_t *shm)
{
    if (shm->producers.list) {
        free(shm->producers.list);
        shm->producers.list = NULL;
    }
    if (shm->consumers.list) {
        free(shm->consumers.list);
        shm->consumers.list = NULL;
    }

    shm->consumers.num = 0;
    shm->producers.num = 0;
}


static ri_channel_t *get_channel(ri_shm_t *shm, unsigned idx)
{
    ri_channel_t *chn = NULL;
    if (shm->owner)
    {
        if (idx < shm->consumers.num) {
            chn = &shm->consumers.list[idx].chn;
        } else {
            idx -= shm->consumers.num;

            if (idx < shm->producers.num)
                chn = &shm->producers.list[idx].chn;
        }
    } else {
        if (idx < shm->producers.num) {
            chn = &shm->producers.list[idx].chn;
        } else {
            idx -= shm->producers.num;

            if (idx < shm->consumers.num)
                chn = &shm->consumers.list[idx].chn;
        }
    }

    return chn;
}


int init_shm(ri_shm_t *shm, const size_t cns_sizes[], const size_t prd_sizes[])
{
    unsigned n_cns = count_channels(cns_sizes);
    unsigned n_prd = count_channels(prd_sizes);

    size_t tbl_offset = mem_align(sizeof(shm_hdr_t), alignof(tbl_entry_t));

    if (tbl_offset >= shm->size) {
        LOG_ERR("header size(%zu) exeeds shm size(%zu)", tbl_offset, shm->size);
    }

    size_t tbl_size = (n_cns + n_prd) * sizeof(tbl_entry_t);
    size_t offset = mem_align(tbl_offset + tbl_size, mem_alignment());

    if (offset >= shm->size) {
        LOG_ERR("table (%zu) doesn't fit in shm (%zu)", offset, shm->size);
    }

    tbl_entry_t *tbl = mem_offset(shm->p, tbl_offset);

    for (unsigned i = 0; i < n_cns; i++) {
        offset = map_channel(&tbl[i], offset, cns_sizes[i], shm->size);
        if (offset == 0) {
            LOG_ERR("rx channel[%u] doesn't fit in shm", i);
            return -ENOMEM;
        }
    }

    for (unsigned i = 0; i < n_prd; i++) {
        offset = map_channel(&tbl[n_cns + i], offset, prd_sizes[i], shm->size);
        if (offset == 0) {
            LOG_ERR("tx channel[%u] doesn't fit in shm", i);
            return -ENOMEM;
        }
    }

    shm_hdr_t *hdr = shm->p;

    *hdr = (shm_hdr_t) {
        .align = mem_alignment(),
        .n_cns = n_cns,
        .n_prd = n_prd,
        .xchg_size = sizeof(ri_xchg_t),
        .tbl_offset = tbl_offset,
        .tbl_size = tbl_size,
        .magic = MAGIC,
    };

    LOG_INF("mapped %u server-to-client channels and %u client-to-server channels; size used=%zu", n_cns, n_prd, offset);

    return 0;
}


static size_t init_channel(ri_channel_t *chn, ri_xchg_t *xchg, void *p, size_t offset, size_t buf_size)
{
    chn->xchg = xchg;

    for (int i = 0; i < RI_NUM_BUFFERS; i++) {
        chn->bufs[i] = mem_offset(p, offset);
        offset += buf_size;
    }

    return offset;
}


static int init_channels(ri_shm_t *shm)
{
    delete_channels(shm);
    int r = -1;

    if (sizeof(shm_hdr_t) > shm->size) {
        LOG_ERR("header (size=%zu) doesn't fit in shm (%zu)", sizeof(shm_hdr_t), shm->size);
        r = -ENOMEM;
        goto fail;
    }

    shm_hdr_t *hdr = shm->p;

    if (!check_hdr(hdr)) {
        r = -EPROTO;
        goto fail;
    }

    size_t tbl_offset = hdr->tbl_offset;

    if (tbl_offset >= shm->size) {
        LOG_ERR("table size(%zu) doesn't fit in shm (%zu)", tbl_offset, shm->size);
        r = -ENOMEM;
        goto fail;
    }

    if (shm->owner) {
        shm->consumers.num = hdr->n_cns;
        shm->producers.num = hdr->n_prd;
    } else {
        shm->consumers.num = hdr->n_prd;
        shm->producers.num = hdr->n_cns;
    }

    shm->consumers.list = malloc(shm->consumers.num * sizeof(ri_consumer_t));

    if (!shm->consumers.list) {
        r = -ENOMEM;
        goto fail;
    }

    shm->producers.list = malloc(shm->producers.num * sizeof(ri_producer_t));

    if (!shm->producers.list) {
        r = -ENOMEM;
        goto fail;
    }

    for (unsigned i = 0; i < shm->producers.num; i++) {
        shm->producers.list[i].current = RI_BUFIDX_NONE;
        shm->producers.list[i].locked = RI_BUFIDX_NONE;
    }

    tbl_entry_t *tbl = mem_offset(shm->p, tbl_offset);
    size_t offset = tbl[0].offset;

    for (unsigned i = 0; i < shm->consumers.num + shm->producers.num; i++) {
        tbl_entry_t *entry = &tbl[i];

        if (entry->offset != offset) {
            LOG_ERR("corrupt channel table at entry %u offset=%zu table=%zu", i, offset, entry->offset);
            goto fail;
        }

        ri_channel_t *chn = get_channel(shm, i);

        if (!chn) {
            LOG_ERR("no channel table entry %u", i);
            goto fail;
        }

        offset = init_channel(chn, &entry->xchg, shm->p, offset, entry->buf_size);

        if (offset > shm->size) {
            LOG_ERR("channel %u exceeds shm size(%zu) offset=%zu", i, shm->size, offset);
            goto fail;
        }
    }

    return 0;

fail:
    delete_channels(shm);
    return r;
}


static ri_shm_t* shm_new(const size_t cns_sizes[], const size_t prd_sizes[], const char *name, mode_t mode)
{
    ri_shm_t *shm = NULL;

    size_t shm_size = calc_shm_size(cns_sizes, prd_sizes);

    if (name)
        shm = ri_sys_named_shm_new(shm_size, name, mode);
    else
        shm = ri_sys_anon_shm_new(shm_size);

    if (!shm)
        return NULL;

    int r = init_shm(shm, cns_sizes, prd_sizes);

    if (r < 0)
        goto fail_init;

    r = init_channels(shm);

    if (r < 0)
        goto fail_init;

    return shm;

fail_init:
    ri_shm_delete(shm);
    return NULL;
}


ri_shm_t* ri_anon_shm_new(const size_t cns_sizes[], const size_t prd_sizes[])
{
    return shm_new(cns_sizes, prd_sizes, NULL, 0);
}


ri_shm_t* ri_named_shm_new(const size_t cns_sizes[], const size_t prd_sizes[], const char *name, mode_t mode)
{
    if (!name)
        return NULL;

    return shm_new(cns_sizes, prd_sizes, name, mode);
}


ri_shm_t* ri_shm_map(int fd)
{
    ri_shm_t *shm = ri_sys_map_shm(fd);

    if (!shm)
        return NULL;

    int r = init_channels(shm);

    if (r < 0) {
        ri_sys_shm_delete(shm);
        return NULL;
    }

    return shm;
}


ri_shm_t* ri_named_shm_map(const char *name)
{
    ri_shm_t *shm = ri_sys_map_named_shm(name);

    if (!shm)
        return NULL;

    int r = init_channels(shm);

    if (r < 0) {
        ri_sys_shm_delete(shm);
        return NULL;
    }

    return shm;
}


void ri_shm_delete(ri_shm_t *shm)
{
    delete_channels(shm);
    ri_sys_shm_delete(shm);
}


ri_consumer_t* ri_shm_get_consumer(const ri_shm_t *shm, unsigned cns_id)
{
    if (cns_id >= shm->consumers.num)
        return NULL;

    return &shm->consumers.list[cns_id];
}


ri_producer_t* ri_shm_get_producer(const ri_shm_t *shm, unsigned prd_id)
{
    if (prd_id >= shm->producers.num)
        return NULL;

    return &shm->producers.list[prd_id];
}


int ri_shm_get_fd(const ri_shm_t* shm)
{
    return shm->fd;
}
