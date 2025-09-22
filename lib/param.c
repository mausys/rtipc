#include "param.h"
#include "index.h"
#include "mem_utils.h"

static size_t ri_calc_data_size(unsigned n_msgs, size_t msg_size)
{
  return n_msgs * cacheline_aligned(msg_size);
}


size_t ri_calc_queue_size(unsigned n_msgs)
{
  unsigned  n = n_msgs + 2; /* tail + head*/


  return cacheline_aligned(n * sizeof(ri_atomic_index_t));
}

size_t ri_calc_channel_size(unsigned n_msgs, size_t msg_size)
{
  /* tail + head + queue*/
  return ri_calc_queue_size(n_msgs) + ri_calc_data_size(n_msgs, msg_size);
}
