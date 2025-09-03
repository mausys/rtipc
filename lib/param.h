#pragma once

#include "rtipc.h"


#define RI_CHANNEL_MIN_MSGS 3


static inline unsigned ri_calc_queue_len(const ri_channel_param_t *param)
{
  return RI_CHANNEL_MIN_MSGS + param->add_msgs;
}

size_t ri_calc_queue_size(const ri_channel_param_t *param);
size_t ri_calc_channel_size(const ri_channel_param_t *param);
