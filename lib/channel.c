#include "channel.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

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
  void *cache;
};

static void producer_cache_write(const ri_producer_t *producer) {
  size_t msg_size = ri_producer_queue_msg_size(producer->queue);
  void *msg = ri_producer_queue_msg(producer->queue);
  memcpy(msg, producer->cache, msg_size);
}


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

  if (consumer->eventfd >= 0) {
    ri_set_nonblocking(consumer->eventfd);
  }

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
    ri_set_nonblocking(producer->eventfd);
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


ri_channel_param_t ri_consumer_param(const ri_consumer_t *consumer)
{
  return (ri_channel_param_t) {
      .add_msgs = ri_consumer_len(consumer) - 3,
      .msg_size = ri_consumer_msg_size(consumer),
      .info =  ri_consumer_info(consumer),
      .eventfd = ri_consumer_eventfd(consumer),
  };
}


ri_channel_param_t ri_producer_param(const ri_producer_t *producer)
{
  return (ri_channel_param_t) {
      .add_msgs = ri_producer_len(producer) - 3,
      .msg_size = ri_producer_msg_size(producer),
      .info =  ri_producer_info(producer),
      .eventfd = ri_producer_eventfd(producer),
  };
}


const void* ri_consumer_msg(const ri_consumer_t *consumer)
{
  return ri_consumer_queue_msg(consumer->queue);
}


void* ri_producer_msg(const ri_producer_t *producer)
{
  return producer->cache ? producer->cache : ri_producer_queue_msg(producer->queue);
}


unsigned ri_consumer_len(const ri_consumer_t *consumer)
{
  return ri_consumer_queue_len(consumer->queue);
}


unsigned ri_producer_len(const ri_producer_t *producer)
{
  return ri_producer_queue_len(producer->queue);
}


size_t ri_consumer_msg_size(const ri_consumer_t *consumer)
{
  return ri_consumer_queue_msg_size(consumer->queue);
}


size_t ri_producer_msg_size(const ri_producer_t *producer)
{
  return ri_producer_queue_msg_size(producer->queue);
}


size_t ri_consumer_shm_offset(const ri_consumer_t *consumer)
{
  return consumer->shm_offset;
}


size_t ri_prdoucer_shm_offset(const ri_producer_t *producer)
{
  return producer->shm_offset;
}


int ri_consumer_eventfd(const ri_consumer_t *consumer)
{
  return consumer->eventfd;
}


int ri_producer_eventfd(const ri_producer_t *producer)
{
  return producer->eventfd;
}


int ri_consumer_take_eventfd(ri_consumer_t *consumer)
{
  int fd = consumer->eventfd;
  consumer->eventfd = -1;
  return fd;
}


int ri_producer_take_eventfd(ri_producer_t *producer)
{
  int fd = producer->eventfd;
  producer->eventfd = -1;
  return fd;
}


ri_info_t ri_consumer_info(const ri_consumer_t *consumer)
{
  return  (ri_info_t) {
      .size = consumer->info.size,
      .data = consumer->info.data,
  };
}


ri_info_t ri_producer_info(const ri_producer_t *producer)
{
  return  (ri_info_t) {
      .size = producer->info.size,
      .data = producer->info.data,
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


void ri_producer_free_info(ri_producer_t *producer)
{
  if (producer->info.data) {
    free(producer->info.data);
    producer->info.data = NULL;
    producer->info.size = 0;
  }
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


ri_produce_result_t ri_producer_force_push(ri_producer_t *producer)
{
  if (producer->cache) {
    producer_cache_write(producer);
  }

  ri_produce_result_t r = ri_producer_queue_force_push(producer->queue);

  if ((producer->eventfd >= 0) && (r == RI_PRODUCE_RESULT_SUCCESS)) {
    uint64_t v = 1;
    write(producer->eventfd, &v, sizeof(v));
  }

  return r;
}


ri_produce_result_t ri_producer_try_push(ri_producer_t *producer)
{
  if (producer->cache) {
    if (ri_producer_queue_full(producer->queue))
      return RI_PRODUCE_RESULT_FAIL;

    producer_cache_write(producer);
  }

  ri_produce_result_t r = ri_producer_queue_try_push(producer->queue);

  if ((producer->eventfd >= 0) && (r == RI_PRODUCE_RESULT_SUCCESS)) {
    uint64_t v = 1;
    write(producer->eventfd, &v, sizeof(v));
  }

  return r;
}


int ri_producer_cache_enable(ri_producer_t *producer)
{
  if (producer->cache)
    return 0;

  size_t msg_size = ri_producer_queue_msg_size(producer->queue);

  producer->cache = malloc(msg_size);

  if (!producer->cache)
    return -ENOMEM;

  void *msg = ri_producer_queue_msg(producer->queue);

  memcpy(producer->cache, msg, msg_size);

  return 0;
}


void ri_producer_cache_disable(ri_producer_t *producer)
{
  if (!producer->cache)
    return;

  size_t msg_size = ri_producer_queue_msg_size(producer->queue);
  void *msg = ri_producer_queue_msg(producer->queue);

  memcpy(msg, producer->cache, msg_size);

  free(producer->cache);
  producer->cache = NULL;
}
