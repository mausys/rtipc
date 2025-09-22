#include "queue.h"

#include "log.h"
#include "mem_utils.h"



void ri_queue_init(ri_queue_t *queue, const ri_channel_param_t *param, void* shm)
{
  ri_atomic_index_t *indices = (ri_atomic_index_t *) shm;

  *queue = (ri_queue_t) {
      .n_msgs = ri_param_queue_len(param),
      .msg_size = cacheline_aligned(param->msg_size),
      .tail = &indices[0],
      .head = &indices[1],
      .chain = &indices[2],
      .msgs =  mem_offset(shm, ri_calc_queue_size(param->add_msgs)),
  };
}


void ri_queue_shm_init(ri_queue_t *queue)
{
  atomic_store(queue->tail, RI_INDEX_INVALID);
  atomic_store(queue->head, RI_INDEX_INVALID);
}


void* ri_queue_get_msg(const ri_queue_t *queue, ri_index_t idx)
{
  if (idx >= queue->n_msgs)
    return NULL;

  return mem_offset(queue->msgs, idx * queue->msg_size);
}


void ri_queue_dump(ri_queue_t *queue)
{
  LOG_INF("\t\tqueue n_msgs=%u, msg_size=%zu", queue->n_msgs, queue->msg_size);
  LOG_INF("\t\t\ttail[0x%p]=0x%x", queue->tail, *queue->tail);
  LOG_INF("\t\t\thead[0x%p]=0x%x", queue->head, *queue->head);

  for (unsigned i = 0; i < queue->n_msgs; i++) {
    LOG_INF("\t\t\tqueue[0x%p]=0x%x", &queue->chain[i], queue->chain[i]);
  }

  LOG_INF("\t\t\tmsgs_start_addr=0x%p", queue->msgs);
}
