#include "rtipc/shm.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#include "rtipc/log.h"

#include "mem_utils.h"


#define MAGIC 0x1f0ca3be // lock-free zero-copy atomic triple buffer exchange :)

#define DIRECTION_MASK    0x1
#define LAST_CHANNEL_MASK 0x2

typedef enum {
    CHN_DIR_RX = 0,
    CHN_DIR_TX = 1
} chn_dir_t;

typedef struct {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint8_t xchg_size;
    uint8_t align;
    uint16_t xchg_offset;
    ri_xchg_t xchg;
} chn_hdr_t;


static size_t get_hdr_size(void)
{
    return mem_align(sizeof(chn_hdr_t), mem_alignment());
}


static bool check_hdr(const chn_hdr_t *hdr)
{
    if (hdr->magic != MAGIC) {
        LOG_ERR("invalid magic field: is=%x expected=%x", hdr->magic, MAGIC);
        return false;
    }

    if (hdr->align != (uint8_t)mem_alignment()) {
        LOG_ERR("invalid align field: is=%x expected=%x", hdr->align, (uint8_t)mem_alignment());
        return false;
    }

    if (hdr->xchg_size != (uint8_t)sizeof(hdr->xchg)) {
        LOG_ERR("invalid xchg_size field: is=%x expected=%x", hdr->xchg_size, (uint8_t)sizeof(hdr->xchg));
        return false;
    }

    if (hdr->xchg_offset != (uint16_t)offsetof(chn_hdr_t, xchg)) {
        LOG_ERR("invalid xchg_offset field: is=%x expected=%x", hdr->xchg_offset, (uint16_t)offsetof(chn_hdr_t, xchg));
        return false;
    }

    return true;
}


static void set_hdr(chn_hdr_t *hdr, size_t size, chn_dir_t dir, bool last)
{
    *hdr = (chn_hdr_t) {
        .magic = MAGIC,
        .size = size,
        .xchg = RI_BUFFER_NONE,
        .flags = (uint32_t)dir | (last ? LAST_CHANNEL_MASK : 0),
        .xchg_size = (uint8_t)sizeof(hdr->xchg),
        .align = (uint8_t)mem_alignment(),
        .xchg_offset = (uint16_t)offsetof(chn_hdr_t, xchg),
    };
}



static size_t get_chn(const ri_shm_t *shm, size_t offset, chn_dir_t *dir, bool *last, ri_chnmap_t *map)
{
    if (offset >= shm->size) {
        LOG_ERR("offset exeeds shm size offset=%zu size=%zu", offset, shm->size);
        return 0;
    }

    size_t hdr_size = get_hdr_size();

    size_t max_size = shm->size - offset;

    if (hdr_size > max_size) {
        LOG_ERR("no shm space for header");
        return 0;
    }

    chn_hdr_t *hdr = mem_offset(shm->p, offset);

    if (!check_hdr(hdr))
        return 0;

    size_t total_size = hdr->size;

    if ((total_size > max_size) || (total_size < hdr_size + 3)) {
        LOG_ERR("no shm space for channel");
        return 0;
    }

    size_t body_size = total_size - hdr_size;

    if (body_size % 3) {
        LOG_ERR("invalid buffer size");
        return 0;
    }

    size_t buf_size = body_size / 3;

    if (buf_size & (mem_alignment() - 1)) {
        LOG_ERR("invalid buffer alignment");
        return 0;
    }

    offset += hdr_size;

    // direction is reversed for client
    if (dir)
        *dir = hdr->flags & DIRECTION_MASK ? CHN_DIR_RX : CHN_DIR_TX;

    if (last)
        *last = !!(hdr->flags & LAST_CHANNEL_MASK);

    if (map)
        map->xchg = &hdr->xchg;

    for (int i = 0; i < 3; i++) {
        if (map)
            map->bufs[i] = mem_offset(shm->p, offset);

        offset += buf_size;
    }

    return offset;
}


static size_t set_chn(const ri_shm_t *shm, size_t offset, size_t buf_size, chn_dir_t dir, bool last, ri_chnmap_t *map)
{
    if (offset >= shm->size) {
        LOG_ERR("offset exeeds shm size offset=%zu size=%zu", offset, shm->size);
        return 0;
    }

    size_t hdr_size = get_hdr_size();
    buf_size = mem_align(buf_size, mem_alignment());
    size_t total_size = hdr_size + 3 * buf_size;
    size_t max_size = shm->size - offset;

    if (total_size > max_size) {
        LOG_ERR("no shm space for channel");
        return 0;
    }

    chn_hdr_t *hdr = mem_offset(shm->p, offset);

    set_hdr(hdr, total_size, dir, last);

    map->xchg = &hdr->xchg;

    offset += hdr_size;

    for (int i = 0; i < 3; i++) {
        map->bufs[i] = mem_offset(shm->p, offset);
        offset += buf_size;
    }

    return offset;
}


static int count_chn(const ri_shm_t *shm, chn_dir_t req_dir)
{
    int cnt = 0;
    size_t offset = 0;

    for (;;) {
        chn_dir_t dir;
        bool last;

        offset = get_chn(shm, offset, &dir, &last, NULL);

        if (offset == 0)
            return -1;

        if (dir == req_dir)
            cnt++;

        if (last)
            break;
    }

    return cnt;
}


static size_t next_chn(const ri_shm_t *shm, size_t offset, chn_dir_t req_dir, ri_chnmap_t *map)
{
    for (;;) {
        chn_dir_t dir;
        bool last;

        offset = get_chn(shm, offset, &dir, &last, map);

        if (offset == 0)
            return 0;

        if (dir == req_dir)
            break;

        if (last)
            return 0;
    }

    return offset;
}


size_t ri_shm_calc_chn_size(size_t buffer_size)
{
    buffer_size = mem_align(buffer_size, mem_alignment());
    return  get_hdr_size() + 3 * buffer_size;
}


size_t ri_shm_set_rchn(const ri_shm_t *shm, size_t offset, size_t buf_size, bool last, ri_rchn_t *chn)
{
    return set_chn(shm, offset, buf_size, CHN_DIR_RX, last, &chn->map);
}


size_t ri_shm_set_tchn(const ri_shm_t *shm, size_t offset, size_t buf_size, bool last, ri_tchn_t *chn)
{
    offset = set_chn(shm, offset, buf_size, CHN_DIR_TX, last, &chn->map);

    if (offset != 0)
        ri_tchn_init(chn);

    return offset;
}


size_t ri_shm_get_rchn(const ri_shm_t *shm, size_t offset, ri_rchn_t *chn)
{
    ri_chnmap_t map;

    offset = next_chn(shm, offset, CHN_DIR_RX, &map);

    if (offset != 0)
        chn->map = map;

    return offset;
}


size_t ri_shm_get_tchn(const ri_shm_t *shm, size_t offset, ri_tchn_t *chn)
{
    ri_chnmap_t map;

    offset = next_chn(shm, offset, CHN_DIR_TX, &map);

    if (offset != 0) {
        chn->map = map;
        ri_tchn_init(chn);
    }

    return offset;
}


int ri_shm_count_rchn(const ri_shm_t *shm)
{
    return count_chn(shm, CHN_DIR_RX);
}


int ri_shm_count_tchn(const ri_shm_t *shm)
{
    return count_chn(shm, CHN_DIR_TX);
}

