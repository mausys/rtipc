#pragma once

#include "rtipc.h"


#define RI_CHANNEL_MIN_MSGS 3

size_t ri_calc_queue_size(unsigned n_msgs);
size_t ri_calc_channel_shm_size(unsigned n_msgs, size_t msg_size);


static inline unsigned ri_count_channels(const ri_channel_config_t channels[])
{
  if (!channels)
    return 0;

  unsigned i;

  for (i = 0; channels[i].msg_size != 0; i++)
    ;

  return i;
}


static inline unsigned ri_channel_queue_len(const ri_channel_config_t *channel)
{
  return RI_CHANNEL_MIN_MSGS + channel->add_msgs;
}


static inline size_t ri_channel_queue_size(const ri_channel_config_t *channel)
{
  return ri_calc_queue_size(ri_channel_queue_len(channel));
}


static inline size_t ri_channel_shm_size(const ri_channel_config_t *channel)
{
  return ri_calc_channel_shm_size(ri_channel_queue_len(channel), channel->msg_size);
}

