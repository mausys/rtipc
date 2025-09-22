#pragma once

#include "rtipc.h"


#define RI_CHANNEL_MIN_MSGS 3

size_t ri_calc_queue_size(unsigned n_msgs);
size_t ri_calc_channel_size(unsigned n_msgs, size_t msg_size);


unsigned ri_param_queue_len(const ri_channel_param_t *param)
{
  return RI_CHANNEL_MIN_MSGS + param->add_msgs;
}

size_t ri_param_queue_size(const ri_channel_param_t *param)
{
  return ri_calc_queue_size(ri_param_queue_len(param));
}

size_t ri_param_channel_size(const ri_channel_param_t *param)
{
  return ri_calc_channel_size(ri_param_queue_len(param), param->msg_size);
}
