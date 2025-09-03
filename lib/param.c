#include "param.h"
#include "index.h"
#include "mem_utils.h"

static size_t ri_calc_data_size(const ri_channel_param_t *param)
{
  unsigned n = ri_calc_queue_len(param);

  return n * cacheline_aligned(param->msg_size);
}


size_t ri_calc_queue_size(const ri_channel_param_t *param)
{
  unsigned n = ri_calc_queue_len(param);
  n += 2; /* tail + head*/

  return cacheline_aligned(n * sizeof(ri_atomic_index_t));
}

size_t ri_calc_channel_size(const ri_channel_param_t *param)
{
  /* tail + head + queue*/
  return ri_calc_queue_size(param) + ri_calc_data_size(param);
}
