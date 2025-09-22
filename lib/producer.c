#include "producer.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include "param.h"
#include "queue.h"

#include "log.h"


struct ri_producer_queue
{
  ri_shm_t *shm;
  ri_queue_t queue;

  ri_index_t head; /* last message in chain that can be used by consumer, chain[head] is always INDEX_END */
  ri_index_t current; /* message used by producer, will become head  */
  ri_index_t overrun; /* message used by consumer when tail moved away by producer, will become current when released by consumer */
  ri_index_t chain[]; /* local copy of chain, because queue is read only for consumer */
};


static void chain_store(ri_producer_queue_t *producer, ri_index_t idx, ri_index_t val)
{
  producer->chain[idx] = val;
  ri_queue_chain_store(&producer->queue, idx, val);
}


unsigned ri_producer_queue_len(const ri_producer_queue_t *producer)
{
  return producer->queue.n_msgs;
}

ri_producer_queue_t* ri_producer_queue_new(const ri_channel_param_t *param, ri_shm_t *shm, size_t shm_offset)
{
  unsigned queue_len = ri_param_queue_len(param);
  size_t size = sizeof(ri_producer_queue_t) + queue_len * sizeof(ri_index_t);

  ri_producer_queue_t *producer =  malloc(size);

  if (!producer)
    goto fail_alloc;

  *producer = (ri_producer_queue_t) {
      .shm = shm,
      .current = 0,
      .overrun = RI_INDEX_INVALID,
      .head = RI_INDEX_INVALID,
  };

  void *ptr = ri_shm_ptr(shm, shm_offset);

  if (!ptr)
    goto fail_shm;

  ri_queue_init(&producer->queue, param, ptr);

  for (unsigned i = 0; i < queue_len - 1; i++) {
    chain_store(producer, i, i + 1);
  }

  chain_store(producer, queue_len - 1, 0);

  ri_shm_ref(producer->shm);

  return producer;

fail_shm:
  free(producer);
fail_alloc:
  return NULL;
}

void ri_producer_queue_delete(ri_producer_queue_t* producer)
{
  ri_shm_unref(producer->shm);

  free(producer);
}


size_t ri_producer_queue_msg_size(const ri_producer_queue_t *producer)
{
  return producer->queue.msg_size;
}

static void enqueue_first_msg(ri_producer_queue_t *producer)
{
  ri_queue_t *queue = &producer->queue;

  /* current message is the new end of chain*/
  chain_store(producer, producer->current, RI_INDEX_INVALID);

  ri_queue_tail_store(queue, producer->current);

  producer->head = producer->current;

  /* announce the new head for consumer_get_head */
  ri_queue_head_store(queue, producer->head);
}

/* set the next message as head
* get_next(msgq, producer->current) after this call
* will return INDEX_END */
static void enqueue_msg(ri_producer_queue_t *producer)
{
  ri_queue_t *queue = &producer->queue;

  /* current message is the new end of chain*/
  chain_store(producer, producer->current, RI_INDEX_INVALID);

  /* append current message to the chain */
  chain_store(producer, producer->head, producer->current);

  producer->head = producer->current;

  /* announce the new head for consumer_get_head */
  ri_queue_head_store(queue, producer->head);
}

static bool move_tail(ri_producer_queue_t *producer, ri_index_t tail)
{
  ri_index_t next = producer->chain[tail & RI_INDEX_MASK];

  return ri_queue_tail_compare_exchange(&producer->queue, tail, next);
}

/* try to jump over tail blocked by consumer */
static bool overrun(ri_producer_queue_t *producer, ri_index_t tail)
{
  const ri_queue_t *queue = &producer->queue;

  ri_index_t new_current = producer->chain[tail & RI_INDEX_MASK]; /* next */
  ri_index_t new_tail = producer->chain[new_current];             /* after next */

  if (ri_queue_tail_compare_exchange(queue, tail, new_tail)) {
    producer->overrun = tail & RI_INDEX_MASK;
    producer->current = new_current;

    return true;
  } else {
    /* consumer just released tail, so use it */
    producer->current = tail & RI_INDEX_MASK;

    return false;
  }
}

/* inserts the next message into the queue and
 * if the queue is full, discard the last message that is not
 * used by consumer. Returns pointer to new message */
ri_produce_result_t ri_producer_queue_force_push(ri_producer_queue_t *producer)
{
  ri_index_t next = producer->chain[producer->current];

  if (producer->head == RI_INDEX_INVALID) {
    enqueue_first_msg(producer);
    producer->current = next;
    return RI_PRODUCE_RESULT_SUCCESS;
  }

  ri_queue_t *queue = &producer->queue;

  bool discarded = false;

  enqueue_msg(producer);

  ri_index_t tail = ri_queue_tail_load(queue);

  if (!ri_queue_index_valid(queue, tail & RI_INDEX_MASK))
    return RI_PRODUCE_RESULT_ERROR;

  bool consumed = !!(tail & RI_CONSUMED_FLAG);

  bool full = (next == (tail & RI_INDEX_MASK));

  if (producer->overrun != RI_INDEX_INVALID) {
    /* we overran the consumer and moved the tail, use overran message as
        * soon as the consumer releases it */
    if (consumed) {
      /* consumer released overrun message, so we can use it */
      /* requeue overrun */
      chain_store(producer, producer->overrun, next);

      producer->current = producer->overrun;
      producer->overrun = RI_INDEX_INVALID;
    } else {
      /* consumer still blocks overran message, move the tail again,
             * because the message queue is still full */
      if (move_tail(producer, tail)) {
        producer->current = tail & RI_INDEX_MASK;
        discarded = true;
      } else {
        /* consumer just released overrun message, so we can use it */
        /* requeue overrun */
        chain_store(producer, producer->overrun, next);

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
      if (move_tail(producer, tail)) {
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

  return discarded ? RI_PRODUCE_RESULT_DISCARDED : RI_PRODUCE_RESULT_SUCCESS;
}

/* trys to insert the next message into the queue */
ri_produce_result_t ri_producer_queue_try_push(ri_producer_queue_t *producer)
{
  ri_index_t next = producer->chain[producer->current];

  if (producer->head == RI_INDEX_INVALID) {
    enqueue_first_msg(producer);
    producer->current = next;
    return RI_PRODUCE_RESULT_SUCCESS;
  }

  const ri_queue_t *queue = &producer->queue;

  ri_index_t tail = ri_queue_tail_load(queue);

  if (!ri_queue_index_valid(queue, tail & RI_INDEX_MASK))
    return RI_PRODUCE_RESULT_ERROR;

  bool consumed = !!(tail & RI_CONSUMED_FLAG);

  bool full = (next == (tail & RI_INDEX_MASK));

  if (producer->overrun != RI_INDEX_INVALID) {
    if (consumed) {
      /* consumer released overrun message, so we can use it */
      /* requeue overrun */
      enqueue_msg(producer);

      chain_store(producer, producer->overrun, next);

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

void* ri_producer_queue_msg(ri_producer_queue_t *producer)
{
  return ri_queue_get_msg(&producer->queue, producer->current);
}
