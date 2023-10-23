#include "rtipc/channel.h"

#include <stdint.h>

#include "mem_utils.h"

#define LOCK_FLAG 0x80


bool ri_tchn_ackd(const ri_tchn_t *chn)
{
    unsigned xchg = atomic_load_explicit(chn->map.xchg, memory_order_relaxed);

    return xchg & LOCK_FLAG;
}


void* ri_rchn_fetch(ri_rchn_t *chn)
{
    unsigned old = atomic_fetch_or_explicit(chn->map.xchg, LOCK_FLAG, memory_order_consume);

    ri_buffer_t current = (ri_buffer_t)(old & 0x3);

    if (current == RI_BUFFER_NONE)
        return NULL;

    return chn->map.bufs[current];
}


ri_chnmap_t ri_chnmap(ri_xchg_t *xchg, void *p, size_t buf_size)
{
    size_t offset = 0;

    ri_chnmap_t map;

    map.xchg = xchg;

    for (int i = 0; i < RI_NUM_BUFFERS; i++) {
        map.bufs[i] = mem_offset(p, offset);
        offset += buf_size;
    }

    return map;
}


void ri_rchn_init(ri_rchn_t *chn, const ri_chnmap_t *map)
{
    *chn = (ri_rchn_t) {
        .map = *map,
    };
}


void ri_tchn_init(ri_tchn_t *chn, const ri_chnmap_t *map)
{
    *chn = (ri_tchn_t) {
        .map = *map,
        .current = RI_BUFFER_0,
        .locked = RI_BUFFER_NONE
    };
}


void* ri_tchn_swap(ri_tchn_t *chn)
{
    unsigned old = atomic_exchange_explicit(chn->map.xchg, chn->current, memory_order_release);

    if (old & LOCK_FLAG)
        chn->locked = (ri_buffer_t)(old & 0x3);

    chn->current = (chn->current + 1) % RI_NUM_BUFFERS;

    if (chn->current == chn->locked)
        chn->current = (chn->current + 1) % RI_NUM_BUFFERS;

    return chn->map.bufs[chn->current];
}


size_t ri_chn_buf_size(const ri_chnmap_t *map)
{
    return (size_t) ((uintptr_t)map->bufs[1] - (uintptr_t)map->bufs[0]);
}


size_t ri_tchn_buf_size(const ri_tchn_t *chn)
{
    return ri_chn_buf_size(&chn->map);
}


size_t ri_rchn_buf_size(const ri_rchn_t *chn)
{
    return ri_chn_buf_size(&chn->map);
}
