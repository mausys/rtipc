#include "consumer.h"
#include "channel.h"

size_t ri_consumer_msg_size(const ri_consumer_t *consumer)
{
  return consumer->channel.msg_size;
}

uintptr_t ri_consumer_init(ri_consumer_t *consumer, uintptr_t start, const ri_channel_param_t *size)
{
  *consumer = (ri_consumer_t) {
      .current = 0,
  };

  return ri_channel_init(&consumer->channel, start, size);
}

ri_consume_result_t ri_consumer_flush(ri_consumer_t *consumer)
{
  ri_channel_t *channel = &consumer->channel;

  for (;;) {
    ri_index_t tail = atomic_fetch_or(channel->tail, RI_CONSUMED_FLAG);

    if (tail == RI_INDEX_INVALID) {
      /* or CONSUMED_FLAG doesn't change INDEX_END*/
      return RI_CONSUME_RESULT_NO_MSG;
    }

    if (!ri_channel_index_valid(channel, tail & RI_INDEX_MASK)) {
      return RI_CONSUME_RESULT_ERROR;
    }

    ri_index_t head = atomic_load(channel->head);

    if (!ri_channel_index_valid(channel, head)) {
      return RI_CONSUME_RESULT_ERROR;
    }

    tail |= RI_CONSUMED_FLAG;

    if (ri_channel_tail_compare_exchange(channel, tail, head | RI_CONSUMED_FLAG)) {
      /* only accept head if producer didn't move tail,
           *  otherwise the producer could fill the whole queue and the head could be the
           *  producers current message  */
      consumer->current = head;
      break;
    }
  }

  return RI_CONSUME_RESULT_DISCARDED;
}

ri_consume_result_t ri_consumer_pop(ri_consumer_t *consumer)
{
  ri_channel_t *channel = &consumer->channel;
  ri_index_t tail = ri_channel_tail_fetch_or(channel, RI_CONSUMED_FLAG);

  if (tail == RI_INDEX_INVALID)
    return RI_CONSUME_RESULT_NO_MSG;

  if (!ri_channel_index_valid(channel, tail & RI_INDEX_MASK))
    return RI_CONSUME_RESULT_ERROR;

  if ((tail & RI_CONSUMED_FLAG) == 0) {
    /* producer moved tail (force_push), so use it; one or more messages were discarded */
    consumer->current = tail;
    return RI_CONSUME_RESULT_DISCARDED;
  }

  /* try to get next message */
  ri_index_t next = ri_channel_queue_load(channel, consumer->current);

  if (next == RI_INDEX_INVALID)
    /* end of queue, no newer message available */
    return RI_CONSUME_RESULT_NO_UPDATE;

  if (!ri_channel_index_valid(channel, next))
    return RI_CONSUME_RESULT_ERROR;

  if (ri_channel_tail_compare_exchange(channel, tail, next | RI_CONSUMED_FLAG)) {
    consumer->current = next;
    return RI_CONSUME_RESULT_SUCCESS;
  } else {
    /* producer just moved tail, use it */
    ri_index_t current = ri_channel_tail_fetch_or(channel, RI_CONSUMED_FLAG);

    if (!ri_channel_index_valid(channel, current))
      return RI_CONSUME_RESULT_ERROR;

    consumer->current = current;

    return RI_CONSUME_RESULT_DISCARDED;
  }
}

const void* ri_consumer_msg(ri_consumer_t *consumer)
{
  if (consumer->current == RI_INDEX_INVALID)
    return NULL;

  return ri_channel_get_msg(&consumer->channel, consumer->current);
}
