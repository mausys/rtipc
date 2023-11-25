#include "types.h"

#include <stdint.h>

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


bool ri_producer_ackd(const ri_producer_t *producer)
{
    unsigned xchg = atomic_load_explicit(producer->channel.xchg, memory_order_relaxed);

    return !!(xchg & LOCK_FLAG);
}


void* ri_consumer_fetch(ri_consumer_t *consumer)
{
    unsigned old = atomic_fetch_or_explicit(consumer->channel.xchg, LOCK_FLAG, memory_order_consume);

    ri_bufidx_t current = (ri_bufidx_t)(old & 0x3);

    if (current == RI_BUFIDX_NONE)
        return NULL;

    return consumer->channel.bufs[current];
}


void* ri_producer_swap(ri_producer_t *producer)
{
    unsigned old = atomic_exchange_explicit(producer->channel.xchg, producer->current, memory_order_release);

    if (old & LOCK_FLAG)
        producer->locked = (ri_bufidx_t)(old & 0x3);

    producer->current = ri_bufidx_inc(producer->current);

    if (producer->current == producer->locked)
        producer->current = ri_bufidx_inc(producer->current);

    return producer->channel.bufs[producer->current];
}


size_t ri_consumer_get_buffer_size(const ri_consumer_t *consumer)
{
    const ri_channel_t *channel = &consumer->channel;
    return (size_t) ((uintptr_t)channel->bufs[1] - (uintptr_t)channel->bufs[0]);
}


size_t ri_producer_get_buffer_size(const ri_producer_t *producer)
{
    const ri_channel_t *channel = &producer->channel;
    return (size_t) ((uintptr_t)channel->bufs[1] - (uintptr_t)channel->bufs[0]);
}

ri_span_t ri_consumer_get_meta(const ri_consumer_t *consumer)
{
    return consumer->channel.meta;
}

ri_span_t ri_producer_get_meta(const ri_producer_t *producer)
{
    return producer->channel.meta;
}

