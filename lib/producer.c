#include "producer.h"

#include <stdbool.h>
#include <assert.h>

#include "log.h"

size_t ri_producer_msg_size(const ri_producer_t *producer)
{
  return producer->channel.msg_size;
}


/* set the next message as head
* get_next(msgq, producer->current) after this call
* will return INDEX_END */
static void enqueue_msg(ri_producer_t *producer)
{
    ri_channel_t *channel = &producer->channel;

    /* current message is the new end of chain*/
    atomic_store(&channel->queue[producer->current], RI_INDEX_INVALID);

    if (producer->head == RI_INDEX_INVALID) {
        /* first message */
        atomic_store(channel->tail, producer->current);
    } else {
        /* append current message to the chain */
        atomic_store(&channel->queue[producer->head], producer->current);
    }

    producer->head = producer->current;

    /* announce the new head for consumer_get_head */
    atomic_store(channel->head, producer->head);
}


static bool move_tail(ri_channel_t *channel, ri_index_t tail)
{
    ri_index_t next = ri_channel_get_next(channel, tail & RI_INDEX_MASK);

    return atomic_compare_exchange_strong(channel->tail, &tail, next);
}

/* try to jump over tail blocked by consumer */
static bool overrun(ri_producer_t *producer, ri_index_t tail)
{
    ri_channel_t *channel = &producer->channel;
    ri_index_t new_current = ri_channel_get_next(channel, tail & RI_INDEX_MASK); /* next */
    ri_index_t new_tail = ri_channel_get_next(channel, new_current); /* after next */

    /* if atomic_compare_exchange_strong fails expected will be overwritten */
    ri_index_t expected = tail;

    if (atomic_compare_exchange_strong(channel->tail, &expected, new_tail)) {
        producer->overrun = tail & RI_INDEX_MASK;
        producer->current = new_current;

        return true;
    } else {
        /* consumer just released tail, so use it */
        producer->current = tail & RI_INDEX_MASK;

        return false;
    }
}

uintptr_t ri_producer_init(ri_producer_t *producer, uintptr_t start, const ri_channel_param_t *size)
{
    *producer = (ri_producer_t) {
        .current = 0,
        .overrun = RI_INDEX_INVALID,
        .head = RI_INDEX_INVALID,
    };

    return ri_channel_init(&producer->channel, start, size);
}

/* inserts the next message into the queue and
 * if the queue is full, discard the last message that is not
 * used by consumer. Returns pointer to new message */
ri_produce_result_t ri_producer_force_push(ri_producer_t *producer)
{
    ri_channel_t *channel = &producer->channel;
    bool discarded = false;

    ri_index_t next = ri_channel_get_next(channel, producer->current);

    enqueue_msg(producer);

    ri_index_t tail = atomic_load(channel->tail);

    bool consumed = !!(tail & RI_CONSUMED_FLAG);

    bool full = (next == (tail & RI_INDEX_MASK));

    /* only for testing */
    ri_index_t old_current = producer->current;

    if (producer->overrun != RI_INDEX_INVALID) {
        /* we overran the consumer and moved the tail, use overran message as
        * soon as the consumer releases it */
        if (consumed) {
            /* consumer released overrun message, so we can use it */
            /* requeue overrun */
            atomic_store(&channel->queue[producer->overrun], next);

            producer->current = producer->overrun;
            producer->overrun = RI_INDEX_INVALID;
        } else {
            /* consumer still blocks overran message, move the tail again,
             * because the message queue is still full */
            if (move_tail(channel, tail)) {
                producer->current = tail & RI_INDEX_MASK;
                discarded = true;
            } else {
                /* consumer just released overrun message, so we can use it */
                /* requeue overrun */
                atomic_store(&channel->queue[producer->overrun], next);

                producer->current = producer->overrun;
                producer->overrun = RI_INDEX_INVALID;
            }
        }
    } else {
        /* no previous overrun, use next or after next message */
        if (!full) {
            /* message queue not full, simply use next */
            producer->current = next;
        } else if (!consumed) {
            /* message queue is full, but no message is consumed yet, so try to move tail */
            if (move_tail(channel, tail)) {
                /* message queue is full -> tail & INDEX_MASK == next */
                producer->current = next;
                discarded = true;
            } else {
                /*  consumer just started and consumed tail
                *  we're assuming that consumer flagged tail (tail | CONSUMED_FLAG),
                *  if this this is not the case, consumer already moved on
                *  and we will use tail  */
                discarded = overrun(producer, tail | RI_CONSUMED_FLAG);
            }
        } else {
            /* overrun the consumer, if the consumer keeps tail */
            discarded = overrun(producer, tail);
        }
    }

    if (old_current == producer->current) {
        LOG_ERR("old_current == producer->current 0x%x %i", old_current, discarded);
    }

    return discarded ? RI_PRODUCE_RESULT_DISCARDED : RI_PRODUCE_RESULT_SUCCESS;
}



/* trys to insert the next message into the queue */
ri_produce_result_t ri_producer_try_push(ri_producer_t *producer)
{
    ri_channel_t *channel = &producer->channel;

    ri_index_t next = ri_channel_get_next(channel, producer->current);

    ri_index_t tail = atomic_load(channel->tail);

    bool consumed = !!(tail & RI_CONSUMED_FLAG);

    bool full = (next == (tail & RI_INDEX_MASK));

    if (producer->overrun != RI_INDEX_INVALID) {
        if (consumed) {
            /* consumer released overrun message, so we can use it */
            /* requeue overrun */
            enqueue_msg(producer);

            atomic_store(&channel->queue[producer->overrun], next);

            producer->current = producer->overrun;
            producer->overrun = RI_INDEX_INVALID;

            return RI_PRODUCE_RESULT_SUCCESS;
        }
    } else {
        /* no previous overrun, use next or after next message */
        if (!full) {
            enqueue_msg(producer);

            producer->current = next;

            return RI_PRODUCE_RESULT_SUCCESS;
        }
    }

    return RI_PRODUCE_RESULT_FAIL;
}


void* ri_producer_msg(ri_producer_t *producer)
{
    return ri_channel_get_msg(&producer->channel, producer->current);
}
