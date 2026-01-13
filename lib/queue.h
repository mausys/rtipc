#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rtipc.h"

#include "index.h"


typedef struct ri_queue
{
  /* number of messages available, can't be lesser than 3 */
  unsigned n_msgs;
  size_t msg_size;
  size_t msg_size_aligned;
  void* msgs;
  /* producer and consumer can change the tail
    *  the MSB shows who has last modified the tail */
  ri_atomic_index_t *tail;
  /* head is only written by producer and only used
    * in consumer_get_head */
  ri_atomic_index_t *head;
  /* circular queue for ordering the messages,
    * initialized simple as queue[i] = (i + 1) % n,
    * but due to overruns might get scrambled.
    * only producer can modify the queue */
  ri_atomic_index_t *chain;
} ri_queue_t;





void* ri_queue_get_msg(const ri_queue_t *queue, ri_index_t idx);

void ri_queue_init(ri_queue_t *queue, const ri_channel_t *channel, void* shm);

void ri_queue_init_shm(const ri_queue_t *queue);

void ri_queue_dump(ri_queue_t *queue);

static inline ri_index_t ri_queue_tail_load(const ri_queue_t *queue)
{
  return atomic_load(queue->tail);
}

static inline void ri_queue_tail_store(const ri_queue_t *queue, ri_index_t val)
{
  atomic_store(queue->tail, val);
}

static inline ri_index_t ri_queue_tail_fetch_or(const ri_queue_t *queue, ri_index_t val)
{
  return atomic_fetch_or(queue->tail, val);
}

static inline bool ri_queue_tail_compare_exchange(const ri_queue_t *queue,
                                                    ri_index_t expected,
                                                    ri_index_t desired)
{
  return atomic_compare_exchange_strong(queue->tail, &expected, desired);
}

static inline ri_index_t ri_queue_head_load(const ri_queue_t *queue)
{
  return atomic_load(queue->head);
}

static inline void ri_queue_head_store(const ri_queue_t *queue, ri_index_t val)
{
  atomic_store(queue->head, val);
}

static inline ri_index_t ri_queue_chain_load(const ri_queue_t *queue, ri_index_t idx)
{
  return atomic_load(&queue->chain[idx]);
}

static inline void ri_queue_chain_store(const ri_queue_t *queue,
                                          ri_index_t idx,
                                          ri_index_t val)
{
  atomic_store(&queue->chain[idx], val);
}

static inline bool ri_queue_index_valid(const ri_queue_t *queue, ri_index_t idx)
{
  return idx < queue->n_msgs;
}
