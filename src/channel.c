#include "rtipc/channel.h"

#include <stdint.h>


#define LOCK_FLAG 0x80




bool ri_tchn_ackd(const ri_tchn_t *chn)
{
    unsigned xchg = atomic_load_explicit(chn->map.xchg, memory_order_relaxed);

    return xchg & LOCK_FLAG;
}


void* ri_rchn_update(ri_rchn_t *chn)
{
    unsigned old = atomic_fetch_or_explicit(chn->map.xchg, LOCK_FLAG, memory_order_consume);

    ri_buffer_t current = (ri_buffer_t)(old & 0x3);

    if (current == RI_BUFFER_NONE)
        return NULL;

    return chn->map.bufs[current];
}

void ri_tchn_init(ri_tchn_t *chn)
{
    chn->current = RI_BUFFER_0;
    chn->locked = RI_BUFFER_NONE;
}

void* ri_tchn_update(ri_tchn_t *chn)
{
    unsigned old = atomic_exchange_explicit(chn->map.xchg, chn->current, memory_order_release);

    if (old & LOCK_FLAG)
        chn->locked = (ri_buffer_t)(old & 0x3);

    chn->current = (chn->current + 1) % 3;

    if (chn->current == chn->locked)
        chn->current = (chn->current + 1) % 3;

    return chn->map.bufs[chn->current];
}

size_t ri_chn_size(const ri_chnmap_t *map)
{
    return (size_t) ((uintptr_t)map->bufs[1] - (uintptr_t)map->bufs[0]);
}


size_t ri_tchn_size(const ri_tchn_t *chn)
{
    return ri_chn_size(&chn->map);
}


size_t ri_rchn_size(const ri_rchn_t *chn)
{
    return ri_chn_size(&chn->map);
}
