#pragma once

#include <stddef.h>
#include <stdint.h>

#include "rtipc.h"

#include "param.h"

#include "index.h"


typedef struct ri_channel
{
  /* number of messages available, can't be lesser than 3 */
  unsigned n_msgs;
  size_t msg_size;
  uintptr_t msgs_start_addr;
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
  ri_atomic_index_t *queue;
} ri_channel_t;





static inline void* ri_channel_get_msg(const ri_channel_t *channel, ri_index_t idx)
{
  if (idx >= channel->n_msgs)
    return NULL;

  return (void*) (channel->msgs_start_addr + (idx * channel->msg_size));
}



void ri_channel_init(ri_channel_t *channel, const ri_channel_param_t *param, uintptr_t start);

void ri_channel_shm_init(ri_channel_t *channel);

void ri_channel_dump(ri_channel_t *channel);

static inline ri_index_t ri_channel_tail_load(const ri_channel_t *channel)
{
  return atomic_load(channel->tail);
}

static inline void ri_channel_tail_store(const ri_channel_t *channel, ri_index_t val)
{
  atomic_store(channel->tail, val);
}

static inline ri_index_t ri_channel_tail_fetch_or(const ri_channel_t *channel, ri_index_t val)
{
  return atomic_fetch_or(channel->tail, val);
}

static inline bool ri_channel_tail_compare_exchange(const ri_channel_t *channel,
                                                    ri_index_t expected,
                                                    ri_index_t desired)
{
  return atomic_compare_exchange_strong(channel->tail, &expected, desired);
}

static inline ri_index_t ri_channel_head_load(const ri_channel_t *channel)
{
  return atomic_load(channel->head);
}

static inline void ri_channel_head_store(const ri_channel_t *channel, ri_index_t val)
{
  atomic_store(channel->head, val);
}

static inline ri_index_t ri_channel_queue_load(const ri_channel_t *channel, ri_index_t idx)
{
  return atomic_load(&channel->queue[idx]);
}

static inline void ri_channel_queue_store(const ri_channel_t *channel,
                                          ri_index_t idx,
                                          ri_index_t val)
{
  atomic_store(&channel->queue[idx], val);
}

static inline bool ri_channel_index_valid(const ri_channel_t *channel, ri_index_t idx)
{
  return idx < channel->n_msgs;
}
