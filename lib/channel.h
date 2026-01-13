#pragma once

#include "rtipc.h"

#include "shm.h"

#define RI_CHANNEL_MIN_MSGS 3

size_t ri_calc_queue_size(unsigned n_msgs);
size_t ri_calc_channel_shm_size(unsigned n_msgs, size_t msg_size);


static inline unsigned ri_count_channels(const ri_channel_t channels[])
{
  if (!channels)
    return 0;

  unsigned i;

  for (i = 0; channels[i].msg_size != 0; i++)
    ;

  return i;
}


static inline unsigned ri_channel_queue_len(const ri_channel_t *channel)
{
  return RI_CHANNEL_MIN_MSGS + channel->add_msgs;
}


static inline size_t ri_channel_queue_size(const ri_channel_t *channel)
{
  return ri_calc_queue_size(ri_channel_queue_len(channel));
}


static inline size_t ri_channel_shm_size(const ri_channel_t *channel)
{
  return ri_calc_channel_shm_size(ri_channel_queue_len(channel), channel->msg_size);
}


size_t ri_calc_shm_size(const ri_channel_t consumers[], const ri_channel_t producers[]);

ri_consumer_t* ri_consumer_new(ri_channel_t *channel, ri_shm_t *shm, size_t shm_offset);
ri_producer_t* ri_producer_new(ri_channel_t *channel, ri_shm_t *shm, size_t shm_offset);

void ri_consumer_delete(ri_consumer_t *consumer);
void ri_producer_delete(ri_producer_t *producer);

void ri_consumer_init_shm(const ri_consumer_t *consumer);
void ri_producer_init_shm(const ri_producer_t *producer);

size_t ri_consumer_shm_offset(const ri_consumer_t *consumer);

unsigned ri_consumer_len(const ri_consumer_t *consumer);

size_t ri_consumer_msg_size(const ri_consumer_t *consumer);

ri_info_t ri_consumer_info(const ri_consumer_t *consumer);

int ri_consumer_eventfd(const ri_consumer_t *consumer);

size_t ri_prdoucer_shm_offset(const ri_producer_t *producer);

unsigned ri_producer_len(const ri_producer_t *producer);

size_t ri_producer_msg_size(const ri_producer_t *producer);

ri_info_t ri_producer_info(const ri_producer_t *producer);

int ri_producer_eventfd(const ri_producer_t *producer);
