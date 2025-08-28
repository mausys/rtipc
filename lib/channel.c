#include "channel.h"

#include "log.h"
#include "mem_utils.h"

static size_t channel_data_size(const ri_channel_param_t *size)
{
  unsigned n = RI_CHANNEL_MIN_MSGS + size->add_msgs;

  return n * cacheline_aligned(size->msg_size);
}

static size_t channel_queue_size(const ri_channel_param_t *size)
{
  unsigned n = RI_CHANNEL_MIN_MSGS + size->add_msgs;
  n += 2; /* tail + head*/

  return cacheline_aligned(n * sizeof(ri_atomic_index_t));
}

size_t ri_channel_calc_size(const ri_channel_param_t *size)
{
  /* tail + head + queue*/
  return channel_queue_size(size) + channel_data_size(size);
}

uintptr_t ri_channel_init(ri_channel_t *channel, uintptr_t start, const ri_channel_param_t *size)
{
  ri_atomic_index_t *indices = (ri_atomic_index_t *) start;

  *channel = (ri_channel_t) {
      .n_msgs = RI_CHANNEL_MIN_MSGS + size->add_msgs,
      .msg_size = cacheline_aligned(size->msg_size),
      .tail = &indices[0],
      .head = &indices[1],
      .queue = &indices[2],
      .msgs_start_addr = start + channel_queue_size(size),
  };

  return start + ri_channel_calc_size(size);
}

void ri_channel_shm_init(ri_channel_t *channel)
{
  unsigned last = channel->n_msgs - 1;

  atomic_store(channel->tail, RI_INDEX_INVALID);
  atomic_store(channel->head, RI_INDEX_INVALID);

  for (unsigned i = 0; i < last; i++) {
    atomic_store(&channel->queue[i], i + 1);
  }

  /* wrap around */
  atomic_store(&channel->queue[last], 0);
}

void ri_channel_dump(ri_channel_t *channel)
{
  LOG_INF("\t\tchannel n_msgs=%u, msg_size=%zu", channel->n_msgs, channel->msg_size);
  LOG_INF("\t\t\ttail[0x%p]=0x%x", channel->tail, *channel->tail);
  LOG_INF("\t\t\thead[0x%p]=0x%x", channel->head, *channel->head);

  for (unsigned i = 0; i < channel->n_msgs; i++) {
    LOG_INF("\t\t\tqueue[0x%p]=0x%x", &channel->queue[i], channel->queue[i]);
  }

  LOG_INF("\t\t\tmsgs_start_addr=0x%p", (void*) channel->msgs_start_addr);
}
