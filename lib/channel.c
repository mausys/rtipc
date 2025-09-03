#include "channel.h"

#include "log.h"
#include "mem_utils.h"



void ri_channel_init(ri_channel_t *channel, const ri_channel_param_t *param, uintptr_t start)
{
  ri_atomic_index_t *indices = (ri_atomic_index_t *) start;

  *channel = (ri_channel_t) {
      .n_msgs = ri_calc_queue_len(param),
      .msg_size = cacheline_aligned(param->msg_size),
      .tail = &indices[0],
      .head = &indices[1],
      .queue = &indices[2],
      .msgs_start_addr = start + ri_calc_queue_size(param),
  };
}

void ri_channel_shm_init(ri_channel_t *channel)
{
  atomic_store(channel->tail, RI_INDEX_INVALID);
  atomic_store(channel->head, RI_INDEX_INVALID);
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
