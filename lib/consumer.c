#include "consumer.h"

#include <stdlib.h>

#include "queue.h"

struct ri_consumer
{
  ri_shm_t *shm;
  ri_queue_t queue;
  ri_index_t current;
};


size_t ri_consumer_msg_size(const ri_consumer_t *consumer)
{
  return consumer->queue.msg_size;
}

ri_consumer_t* ri_consumer_new(ri_shm_t *shm, const ri_channel_param_t *param, uintptr_t start, bool shm_init)
{
  ri_consumer_t *consumer = malloc(sizeof(ri_consumer_t));

  if (!consumer)
    return NULL;

  *consumer = (ri_consumer_t) {
      .shm = shm,
      .current = 0,
  };

  ri_queue_init(&consumer->queue, param, start);

  if (shm_init)
    ri_queue_shm_init(&consumer->queue);

  ri_shm_ref(consumer->shm);
  return consumer;
}


void ri_consumer_delete(ri_consumer_t *consumer)
{
  ri_shm_unref(consumer->shm);
  free(consumer);
}

ri_consume_result_t ri_consumer_flush(ri_consumer_t *consumer)
{
  ri_queue_t *queue = &consumer->queue;

  for (;;) {
    ri_index_t tail = atomic_fetch_or(queue->tail, RI_CONSUMED_FLAG);

    if (tail == RI_INDEX_INVALID) {
      /* or CONSUMED_FLAG doesn't change INDEX_END*/
      return RI_CONSUME_RESULT_NO_MSG;
    }

    if (!ri_queue_index_valid(queue, tail & RI_INDEX_MASK)) {
      return RI_CONSUME_RESULT_ERROR;
    }

    ri_index_t head = atomic_load(queue->head);

    if (!ri_queue_index_valid(queue, head)) {
      return RI_CONSUME_RESULT_ERROR;
    }

    tail |= RI_CONSUMED_FLAG;

    if (ri_queue_tail_compare_exchange(queue, tail, head | RI_CONSUMED_FLAG)) {
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
  ri_queue_t *queue = &consumer->queue;
  ri_index_t tail = ri_queue_tail_fetch_or(queue, RI_CONSUMED_FLAG);

  if (tail == RI_INDEX_INVALID)
    return RI_CONSUME_RESULT_NO_MSG;

  if (!ri_queue_index_valid(queue, tail & RI_INDEX_MASK))
    return RI_CONSUME_RESULT_ERROR;

  if ((tail & RI_CONSUMED_FLAG) == 0) {
    /* producer moved tail (force_push), so use it; one or more messages were discarded */
    consumer->current = tail;
    return RI_CONSUME_RESULT_DISCARDED;
  }

  /* try to get next message */
  ri_index_t next = ri_queue_chain_load(queue, consumer->current);

  if (next == RI_INDEX_INVALID)
    /* end of queue, no newer message available */
    return RI_CONSUME_RESULT_NO_UPDATE;

  if (!ri_queue_index_valid(queue, next))
    return RI_CONSUME_RESULT_ERROR;

  if (ri_queue_tail_compare_exchange(queue, tail, next | RI_CONSUMED_FLAG)) {
    consumer->current = next;
    return RI_CONSUME_RESULT_SUCCESS;
  } else {
    /* producer just moved tail, use it */
    ri_index_t current = ri_queue_tail_fetch_or(queue, RI_CONSUMED_FLAG);

    if (!ri_queue_index_valid(queue, current))
      return RI_CONSUME_RESULT_ERROR;

    consumer->current = current;

    return RI_CONSUME_RESULT_DISCARDED;
  }
}

const void* ri_consumer_msg(ri_consumer_t *consumer)
{
  if (consumer->current == RI_INDEX_INVALID)
    return NULL;

  return ri_queue_get_msg(&consumer->queue, consumer->current);
}
