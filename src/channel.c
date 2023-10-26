#include "rtipc/channel.h"

#include <stdint.h>

#include "mem_utils.h"

#define LOCK_FLAG 0x80


static ri_bufidx_t ri_bufidx_inc(ri_bufidx_t i)
{
    static const ri_bufidx_t lut[] = {
        RI_BUFIDX_1,
        RI_BUFIDX_2,
        RI_BUFIDX_0,
        RI_BUFIDX_0
    };

    return lut[i];
}

bool ri_producer_ackd(const ri_producer_t *prd)
{
    unsigned xchg = atomic_load_explicit(prd->chn.xchg, memory_order_relaxed);

    return !!(xchg & LOCK_FLAG);
}


void* ri_consumer_fetch(ri_consumer_t *cns)
{
    unsigned old = atomic_fetch_or_explicit(cns->chn.xchg, LOCK_FLAG, memory_order_consume);

    ri_bufidx_t current = (ri_bufidx_t)(old & 0x3);

    if (current == RI_BUFIDX_NONE)
        return NULL;

    return cns->chn.bufs[current];
}


ri_channel_t ri_channel_create(ri_xchg_t *xchg, void *p, size_t buf_size)
{
    size_t offset = 0;

    ri_channel_t chn;

    chn.xchg = xchg;

    for (int i = 0; i < RI_NUM_BUFFERS; i++) {
        chn.bufs[i] = mem_offset(p, offset);
        offset += buf_size;
    }

    return chn;
}


void ri_consumer_init(ri_consumer_t *cns, const ri_channel_t *chn)
{
    *cns = (ri_consumer_t) {
        .chn = *chn,
    };
}


void ri_producer_init(ri_producer_t *prd, const ri_channel_t *chn)
{
    *prd = (ri_producer_t) {
        .chn = *chn,
        .current = RI_BUFIDX_NONE,
        .locked = RI_BUFIDX_NONE
    };
}


void* ri_producer_swap(ri_producer_t *prd)
{
    unsigned old = atomic_exchange_explicit(prd->chn.xchg, prd->current, memory_order_release);

    if (old & LOCK_FLAG)
        prd->locked = (ri_bufidx_t)(old & 0x3);

    prd->current = ri_bufidx_inc(prd->current);

    if (prd->current == prd->locked)
        prd->current = ri_bufidx_inc(prd->current);

    return prd->chn.bufs[prd->current];
}


size_t ri_channel_get_buffer_size(const ri_channel_t *chn)
{
    return (size_t) ((uintptr_t)chn->bufs[1] - (uintptr_t)chn->bufs[0]);
}


