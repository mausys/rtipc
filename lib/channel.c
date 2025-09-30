#include "channel.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "producer.h"
#include "consumer.h"
#include "param.h"
#include "fd.h"
#include "log.h"

struct ri_consumer {
  ri_consumer_queue_t *queue;
  size_t shm_offset;
  int eventfd;
  struct {
    size_t size;
    void *data;
  } info;
};

struct ri_producer {
  ri_producer_queue_t *queue;
  size_t shm_offset;
  int eventfd;
  struct {
    size_t size;
    void *data;
  } info;
};


ri_consumer_t* ri_consumer_new(const ri_channel_param_t *param, ri_shm_t *shm, size_t shm_offset, int eventfd, bool shm_init)
{
  ri_consumer_t *consumer = malloc(sizeof(ri_consumer_t));

  if (!consumer)
    goto fail_alloc;

  *consumer = (ri_consumer_t) {
    .shm_offset = shm_offset,
    .eventfd = eventfd,
    .info.size = param->info.size,
  };

  if ((param->info.size > 0) && param->info.data) {
    consumer->info.data = malloc(param->info.size);

    if (!consumer->info.data)
      goto fail_info;

    memcpy(consumer->info.data, param->info.data, param->info.size);
  }

  consumer->queue = ri_consumer_queue_new(param, shm, shm_offset);

  if (!consumer->queue)
    goto fail_queue;

  if (shm_init)
    ri_consumer_queue_shm_init(consumer->queue);

  LOG_DBG("consumer created add_msg=%u msg_size=%zu, eventfd=%d shm_offset=%zu", param->add_msgs, param->msg_size, eventfd, shm_offset);

  return consumer;

fail_queue:
  if (consumer->info.data)
    free(consumer->info.data);
fail_info:
  free(consumer);
fail_alloc:
  return NULL;
}


ri_producer_t* ri_producer_new(const ri_channel_param_t *param, ri_shm_t *shm, size_t shm_offset, int eventfd, bool shm_init)
{
  ri_producer_t *producer = malloc(sizeof(ri_producer_t));

  if (!producer)
    goto fail_alloc;

  *producer = (ri_producer_t) {
    .shm_offset = shm_offset,
    .eventfd = eventfd,
    .info.size = param->info.size,
  };

  if ((param->info.size > 0) && param->info.data) {
    producer->info.data = malloc(param->info.size);

    if (!producer->info.data)
      goto fail_info;

    memcpy(producer->info.data, param->info.data, param->info.size);
  }

  producer->queue = ri_producer_queue_new(param, shm, shm_offset);

  if (!producer->queue)
    goto fail_queue;

  if (shm_init)
    ri_producer_queue_shm_init(producer->queue);

  if (producer->eventfd >= 0) {
    int r = ri_set_nonblocking(producer->eventfd);
    LOG_WRN("ri_fd_set_nonblocking failed (%d)", r);
  }

  LOG_DBG("producer created add_msg=%u msg_size=%zu, eventfd=%d shm_offset=%zu", param->add_msgs, param->msg_size, eventfd, shm_offset);

  return producer;

fail_queue:
  if (producer->info.data)
    free(producer->info.data);
fail_info:
  free(producer);
fail_alloc:
  return NULL;
}


void ri_consumer_delete(ri_consumer_t *consumer)
{
  ri_consumer_queue_delete(consumer->queue);

  if (consumer->eventfd >= 0)
    close(consumer->eventfd);

  if (consumer->info.data)
    free(consumer->info.data);

  free(consumer);
}


void ri_producer_delete(ri_producer_t *producer)
{
  ri_producer_queue_delete(producer->queue);

  if (producer->eventfd >= 0)
    close(producer->eventfd);

  if (producer->info.data)
    free(producer->info.data);

  free(producer);
}


size_t ri_consumer_shm_offset(const ri_consumer_t *consumer)
{
  return consumer->shm_offset;
}


unsigned ri_consumer_len(const ri_consumer_t *consumer)
{
  return ri_consumer_queue_len(consumer->queue);
}


const void* ri_consumer_msg(const ri_consumer_t *consumer)
{
  return ri_consumer_queue_msg(consumer->queue);
}


ri_consume_result_t ri_consumer_pop(ri_consumer_t *consumer)
{
  if (consumer->eventfd >= 0) {
    uint64_t v;
    int r = read(consumer->eventfd, &v, sizeof(v));

    if (r < 0) {
      return ri_consumer_queue_msg(consumer->queue)? RI_CONSUME_RESULT_NO_UPDATE : RI_CONSUME_RESULT_NO_MSG;
    }
  }

  return ri_consumer_queue_pop(consumer->queue);
}


ri_consume_result_t ri_consumer_flush(ri_consumer_t *consumer)
{
  ri_consume_result_t r;
  if (consumer->eventfd >= 0) {
    do {
      r = ri_consumer_pop(consumer);
    } while (r == RI_CONSUME_RESULT_SUCCESS);
  } else {
    r = ri_consumer_queue_flush(consumer->queue);
  }

  return r;
}



size_t ri_consumer_msg_size(const ri_consumer_t *consumer)
{
  return ri_consumer_queue_msg_size(consumer->queue);
}


ri_info_t ri_consumer_info(const ri_consumer_t *consumer)
{
  return  (ri_info_t) {
      .size = consumer->info.size,
      .data = consumer->info.data,
  };
}


void ri_consumer_free_info(ri_consumer_t *consumer)
{
  if (consumer->info.data) {
    free(consumer->info.data);
    consumer->info.data = NULL;
    consumer->info.size = 0;
  }
}


int ri_consumer_eventfd(const ri_consumer_t *consumer)
{
  return consumer->eventfd;
}


size_t ri_prdoucer_shm_offset(const ri_producer_t *producer)
{
  return producer->shm_offset;
}

unsigned ri_producer_len(const ri_producer_t *producer)
{
  return ri_producer_queue_len(producer->queue);
}

size_t ri_producer_msg_size(const ri_producer_t *producer)
{
  return ri_producer_queue_msg_size(producer->queue);
}


void* ri_producer_msg(const ri_producer_t *producer)
{
  return ri_producer_queue_msg(producer->queue);
}


ri_produce_result_t ri_producer_force_push(ri_producer_t *producer)
{
  ri_produce_result_t r = ri_producer_queue_force_push(producer->queue);

  if ((producer->eventfd >= 0) && (r == RI_PRODUCE_RESULT_SUCCESS)) {
    uint64_t v = 1;
    write(producer->eventfd, &v, sizeof(v));
  }

  return r;
}

ri_produce_result_t ri_producer_try_push(ri_producer_t *producer)
{
  ri_produce_result_t r = ri_producer_queue_try_push(producer->queue);

  if ((producer->eventfd >= 0) && (r == RI_PRODUCE_RESULT_SUCCESS)) {
    uint64_t v = 1;
    write(producer->eventfd, &v, sizeof(v));
  }

  return r;
}


void ri_producer_free_info(ri_producer_t *producer)
{
  if (producer->info.data) {
    free(producer->info.data);
    producer->info.data = NULL;
    producer->info.size = 0;
  }
}


ri_info_t ri_producer_info(const ri_producer_t *producer)
{
  return  (ri_info_t) {
    .size = producer->info.size,
    .data = producer->info.data,
  };
}


int ri_producer_eventfd(const ri_producer_t *producer)
{
  return producer->eventfd;
}
