#include "channel.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "producer.h"
#include "consumer.h"


struct ri_consumer {
  ri_consumer_queue_t *queue;
  int fd;
  size_t info_size;
  void *info;
};

struct ri_producer {
  ri_producer_queue_t *queue;
  int fd;
  size_t info_size;
  void *info;
};


ri_consumer_t* ri_consumer_new(const ri_channel_param_t *param, ri_shm_t *shm, uintptr_t start, int fd)
{
  ri_consumer_t *consumer = malloc(sizeof(ri_consumer_t));

  if (!consumer)
    goto fail_alloc;

  *consumer = (ri_consumer_t) { .fd = fd };

  if ((param->info.size > 0) && param->info.data) {
    consumer->info = malloc(param->info.size);

    if (!consumer->info)
      goto fail_info;

    memcpy(consumer->info, param->info.data, param->info.size);
  }

  consumer->queue = ri_consumer_queue_new(param, shm, start);

  if (!consumer->queue)
    goto fail_queue;

  return consumer;

fail_queue:
  if (consumer->info)
    free(consumer->info);
fail_info:
  free(consumer);
fail_alloc:
  return NULL;
}


ri_producer_t* ri_producer_new(const ri_channel_param_t *param, ri_shm_t *shm, uintptr_t start, int fd)
{
  ri_producer_t *producer = malloc(sizeof(ri_producer_t));

  if (!producer)
    goto fail_alloc;

   *producer = (ri_producer_t) { .fd = fd };

  if ((param->info.size > 0) && param->info.data) {
    producer->info = malloc(param->info.size);

    if (!producer->info)
      goto fail_info;

    memcpy(producer->info, param->info.data, param->info.size);
  }

  producer->queue = ri_producer_queue_new(param, shm, start);

  if (!producer->queue)
    goto fail_queue;

  return producer;

fail_queue:
  if (producer->info)
    free(producer->info);
fail_info:
  free(producer);
fail_alloc:
  return NULL;
}


void ri_consumer_delete(ri_consumer_t *consumer)
{
  ri_consumer_queue_delete(consumer->queue);

  if (consumer->fd >= 0)
    close(consumer->fd);

  if (consumer->info)
    free(consumer->info);

  free(consumer);
}


void ri_producer_delete(ri_producer_t *producer)
{
  ri_producer_queue_delete(producer->queue);

  if (producer->fd >= 0)
    close(producer->fd);

  if (producer->info)
    free(producer->info);

  free(producer);
}
