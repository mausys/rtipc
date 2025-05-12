#include "consumer.h"
#include "channel.h"


uintptr_t ri_consumer_init(ri_consumer_t *consumer, uintptr_t start, const ri_channel_size_t *size)
{
    *consumer = (ri_consumer_t) {
        .current = 0,
    };

    return ri_channel_init(&consumer->channel, start, size);
}

void* consumer_get_head(ri_consumer_t *consumer)
{
    ri_channel_t *channel = &consumer->channel;

    for (;;) {
        ri_index_t tail = atomic_fetch_or(channel->tail, RI_CONSUMED_FLAG);

        if (tail == RI_INDEX_INVALID) {
            /* or CONSUMED_FLAG doesn't change INDEX_END*/
            return NULL;
        }

        ri_index_t head = atomic_load(channel->head);

        tail |= RI_CONSUMED_FLAG;

        if (atomic_compare_exchange_strong(channel->tail, &tail, head | RI_CONSUMED_FLAG)) {
            /* only accept head if producer didn't move tail,
            *  otherwise the producer could fill the whole queue and the head could be the
            *  producers current message  */
            consumer->current = head;
            break;
        }
    }

    return ri_channel_get_msg(channel, consumer->current);
}


void* ri_consumer_get_tail(ri_consumer_t *consumer)
{
    ri_channel_t *channel = &consumer->channel;
    ri_index_t tail = atomic_fetch_or(channel->tail, RI_CONSUMED_FLAG);

    if (tail == RI_INDEX_INVALID)
        return NULL;

    if (tail & RI_CONSUMED_FLAG) {
        /* try to get next message */
        ri_index_t next = ri_channel_get_next(channel, consumer->current);

        if (next != RI_INDEX_INVALID) {
            if (atomic_compare_exchange_strong(channel->tail, &tail, next | RI_CONSUMED_FLAG)) {
                consumer->current = next;
            } else {
                /* producer just moved tail, use it */
                consumer->current = atomic_fetch_or(channel->tail, RI_CONSUMED_FLAG);
            }
        }
    } else {
        /* producer moved tail, use it*/
        consumer->current = tail;
    }

    if (consumer->current == RI_INDEX_INVALID) {
        /* nothing produced yet */
        return NULL;
    }

    return ri_channel_get_msg(channel, consumer->current);
}
