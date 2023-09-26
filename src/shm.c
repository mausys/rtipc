#include "rtipc/shm.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdalign.h>
#include <errno.h>

#include "rtipc/log.h"

#include "mem_utils.h"


#define MAGIC 0x1f0ca3be // lock-free zero-copy atomic triple buffer exchange :)

#define DIRECTION_MASK    0x1

#define LAST_CHANNEL_MASK 0x2


typedef struct {
    size_t offset;
    size_t buf_size;
    ri_xchg_t xchg;
} tbl_entry_t;


typedef struct {
    uint32_t n_c2s_chns;
    uint32_t n_s2c_chns;
    size_t tbl_offset;
    size_t tbl_size;
    uint16_t align;
    uint8_t xchg_size;
    uint32_t magic;
} shm_hdr_t;


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


static unsigned count_channels(const size_t chns[])
{
    unsigned i;

    for (i = 0; chns[i] != 0; i++)
        ;

    return i;
}


static size_t map_channel(tbl_entry_t *chn, size_t offset, size_t buf_size, size_t shm_size)
{
    buf_size = mem_align(buf_size, mem_alignment());
    size_t chn_size = 3 * buf_size;

    if (offset + chn_size > shm_size) {
        LOG_ERR("channel(size=%zu) doesn't fit in shm(size=%zu)", chn_size, shm_size);
        return 0;
    }

    chn->buf_size = buf_size;
    chn->xchg = RI_BUFFER_NONE;
    chn->offset = offset;

    return  offset + chn_size;
}


static int get_channel(const ri_shm_t *shm, unsigned idx, ri_chn_dir_t dir, ri_chnmap_t *map)
{
    if (sizeof(shm_hdr_t) > shm->size) {
        LOG_ERR("header (size=%zu) doesn't fit in shm (%zu)", sizeof(shm_hdr_t), shm->size);
        return -ENOMEM;
    }

    shm_hdr_t *hdr = shm->p;

    if (!check_hdr(hdr))
        return -EPROTO;

    unsigned num_chns = hdr->n_c2s_chns + hdr->n_s2c_chns;

    if (dir == RI_CHN_S2C)
        idx += hdr->n_c2s_chns;

    if (idx >= num_chns) {
        LOG_ERR("index=%u exeeds channel number (%u)", idx, num_chns);
        return -ENOENT;
    }

    size_t tbl_offset = hdr->tbl_offset;

    if (tbl_offset >= shm->size) {
        LOG_ERR("table size(%zu) doesn't fit in shm (%zu)", tbl_offset, shm->size);
        return -ENOMEM;
    }

    tbl_entry_t *tbl = mem_offset(shm->p, tbl_offset);

    size_t offset = tbl[idx].offset;
    size_t buf_size = tbl[idx].buf_size;

    if (offset + 3 * buf_size > shm->size) {
        LOG_ERR("channel end (%zu) exeeds shm size(%zu)", offset + 3 * buf_size, shm->size);
        return -ENOMEM;
    }

    map->xchg = &tbl[idx].xchg;

    for (int i = 0; i < 3; i++) {
        map->bufs[i] = mem_offset(shm->p, offset);
        offset += buf_size;
    }

    return 0;
}


int ri_shm_get_rx_channel(const ri_shm_t *shm, unsigned idx, ri_chn_dir_t dir, ri_rchn_t *chn)
{
    ri_chnmap_t map;

    int r = get_channel(shm, idx, dir, &map);

    if (r < 0)
        return r;

    ri_rchn_init(chn, &map);

    return 0;
}


int ri_shm_get_tx_channel(const ri_shm_t *shm, unsigned idx, ri_chn_dir_t dir, ri_tchn_t *chn)
{
    ri_chnmap_t map;

    int r = get_channel(shm, idx, dir, &map);

    if (r < 0)
        return r;

    ri_tchn_init(chn, &map);

    return 0;
}


size_t ri_shm_calc_size(const size_t c2s_chn_sizes[], const size_t s2c_chn_sizes[])
{
    unsigned n_c2s_chns = count_channels(c2s_chn_sizes);
    unsigned n_s2c_chns = count_channels(s2c_chn_sizes);

    size_t offset = mem_align(sizeof(shm_hdr_t), alignof(tbl_entry_t));

    size_t tbl_size = (n_c2s_chns + n_s2c_chns) * sizeof(tbl_entry_t);
    offset = mem_align(offset + tbl_size, mem_alignment());

    for (unsigned i = 0; i < n_c2s_chns; i++) {
        size_t buf_size = mem_align(c2s_chn_sizes[i], mem_alignment());
        offset += 3 * buf_size;
    }

    for (unsigned i = 0; i < n_s2c_chns; i++) {
        size_t buf_size = mem_align(s2c_chn_sizes[i], mem_alignment());
        offset += 3 * buf_size;
    }

    return offset;
}



int ri_shm_map_channels(const ri_shm_t *shm, const size_t c2s_chn_sizes[], const size_t s2c_chn_sizes[])
{
    unsigned n_c2s_chns = count_channels(c2s_chn_sizes);
    unsigned n_s2c_chns = count_channels(s2c_chn_sizes);

    size_t tbl_offset = mem_align(sizeof(shm_hdr_t), alignof(tbl_entry_t));

    if (tbl_offset >= shm->size) {
        LOG_ERR("header size(%zu) exeeds shm size(%zu)", tbl_offset, shm->size);
    }

    size_t tbl_size = (n_c2s_chns + n_s2c_chns) * sizeof(tbl_entry_t);
    size_t offset = mem_align(tbl_offset + tbl_size, mem_alignment());

    if (offset >= shm->size) {
        LOG_ERR("table (%zu) doesn't fit in shm (%zu)", offset, shm->size);
    }

    tbl_entry_t *tbl = mem_offset(shm->p, tbl_offset);

    for (unsigned i = 0; i < n_c2s_chns; i++) {
        offset = map_channel(&tbl[i], offset, c2s_chn_sizes[i], shm->size);
        if (offset == 0) {
            LOG_ERR("rx channel[%u] doesn't fit in shm", i);
            return -ENOMEM;
        }
    }

    for (unsigned i = 0; i < n_s2c_chns; i++) {
        offset = map_channel(&tbl[n_c2s_chns + i], offset, s2c_chn_sizes[i], shm->size);
        if (offset == 0) {
            LOG_ERR("tx channel[%u] doesn't fit in shm", i);
            return -ENOMEM;
        }
    }

    shm_hdr_t *hdr = shm->p;

    *hdr = (shm_hdr_t) {
        .align = mem_alignment(),
        .n_c2s_chns = n_c2s_chns,
        .n_s2c_chns = n_s2c_chns,
        .xchg_size = sizeof(ri_xchg_t),
        .tbl_offset = tbl_offset,
        .tbl_size = tbl_size,
        .magic = MAGIC,
    };

    LOG_INF("mapped %u server-to-client channels and %u client-to-server channels; size used=%zu", n_c2s_chns, n_s2c_chns, offset);

    return 0;
}


