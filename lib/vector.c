#include "vector.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "rtipc.h"
#include "param.h"
#include "channel.h"
#include "fd.h"



static size_t calc_shm_size(const ri_channel_t consumers[], const ri_channel_t producers[])
{
  unsigned n_consumers = ri_count_channels(consumers);
  unsigned n_producers = ri_count_channels(producers);

  size_t size = 0;

  for (unsigned i = 0; i < n_consumers; i++)
    size +=  ri_channel_shm_size(&consumers[i]);

  for (unsigned i = 0; i < n_producers; i++)
    size += ri_channel_shm_size(&producers[i]);

  return size;
}


static ri_vector_t* ri_vector_alloc(unsigned n_consumers, unsigned n_producers)
{
  ri_vector_t *vec = calloc(1, sizeof(ri_vector_t));

  if (!vec)
    goto fail_alloc;

  if (n_consumers > 0) {
    vec->consumers = calloc(n_consumers, sizeof(ri_consumer_t*));

    if (!vec->consumers)
      goto fail_consumers;
  }

  if (n_producers > 0) {
    vec->producers = calloc(n_producers, sizeof(ri_producer_t*));

    if (!vec->producers)
      goto fail_producers;
  }

  vec->n_consumers = n_consumers;
  vec->n_producers = n_producers;

  return vec;

fail_producers:
  if (n_consumers > 0)
    free(vec->consumers);
fail_consumers:
  free(vec);
fail_alloc:
  return NULL;
}



void ri_vector_delete(ri_vector_t* vec)
{
  if (vec->consumers) {
    for (unsigned i = 0; i < vec->n_consumers; i++) {
      if (vec->consumers[i]) {
        ri_consumer_delete(vec->consumers[i]);
      }
    }
    free(vec->consumers);
  }

  if (vec->producers) {
    for (unsigned i = 0; i < vec->n_producers; i++) {
      if (vec->producers[i]) {
        ri_producer_delete(vec->producers[i]);
      }
    }
    free(vec->producers);
  }

  if (vec->shm)
    ri_shm_unref(vec->shm);

  free(vec);
}


ri_vector_t* ri_vector_new(const ri_vector_config_t *vconfig)
{
  unsigned n_consumers = ri_count_channels(vconfig->consumers);
  unsigned n_producers = ri_count_channels(vconfig->producers);

  ri_vector_t *vec = ri_vector_alloc(n_consumers, n_producers);

  if (!vec)
    goto fail_alloc;

  size_t shm_size = calc_shm_size(vconfig->consumers, vconfig->producers);

  vec->shm = ri_shm_new(shm_size);

  if (!vec->shm)
    goto fail_shm;

  size_t shm_offset = 0;

  for (unsigned i = 0; i < n_producers; i++) {
    const ri_channel_t *channel = &vconfig->producers[i];
    int fd = -1;

    if (channel->eventfd) {
      fd = ri_eventfd();

      if (fd < 0)
        goto fail_channel;
    }

    vec->producers[i] = ri_producer_new(channel, vec->shm, shm_offset, fd, true);

    if (!vec->producers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);

      goto fail_channel;
    }

    shm_offset += ri_channel_shm_size(channel);
  }

  for (unsigned i = 0; i < n_consumers; i++) {
    const ri_channel_t *channel = &vconfig->consumers[i];
    int fd = -1;

    if (channel->eventfd) {
      fd = ri_eventfd();

      if (fd < 0)
          goto fail_channel;
    }

    vec->consumers[i] = ri_consumer_new(channel, vec->shm, shm_offset, fd, true);

    if (!vec->consumers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);

      goto fail_channel;
    }

    shm_offset += ri_channel_shm_size(channel);
  }

  int r = ri_vector_set_info(vec, &vconfig->info);
    if (r < 0)
      goto fail_channel;

  return vec;

fail_channel:
fail_shm:
  ri_vector_delete(vec);
fail_alloc:
  return NULL;
}


ri_vector_t* ri_vector_map(ri_vector_transfer_t *vxfer)
{
  unsigned n_consumers = ri_count_channels(vxfer->consumers);
  unsigned n_producers = ri_count_channels(vxfer->producers);

  ri_vector_t *vec = ri_vector_alloc(n_consumers, n_producers);

  if (!vec)
    goto fail_alloc;

  int r = ri_check_memfd(vxfer->shmfd);

  if (r < 0) {
    LOG_ERR("memfd check failed");
    goto fail_shm;
  }

  vec->shm = ri_shm_map(vxfer->shmfd);

  if (!vec->shm)
    goto fail_shm;

  vxfer->shmfd = -1;

  size_t shm_offset = 0;

  for (unsigned i = 0; i < n_consumers; i++) {
    ri_channel_t *channel = &vxfer->consumers[i];

    if (channel->eventfd > 0) {
      if (ri_check_eventfd(channel->eventfd) < 0) {
        LOG_ERR("eventfd check failed");
        goto fail_channel;
      }
    }

    vec->consumers[i] = ri_consumer_new(channel, vec->shm, shm_offset, channel->eventfd, false);

    if (!vec->consumers[i])
      goto fail_channel;

    channel->eventfd = -1;
    shm_offset += ri_channel_shm_size(channel);
  }

  for (unsigned i = 0; i < n_producers; i++) {
    ri_channel_t *channel = &vxfer->producers[i];

    if (channel->eventfd > 0) {
      if (ri_check_eventfd(channel->eventfd) < 0) {
        LOG_ERR("eventfd check failed");
        goto fail_channel;
      }
    }

    vec->producers[i] = ri_producer_new(channel, vec->shm, shm_offset, channel->eventfd, false);

    if (!vec->producers[i])
      goto fail_channel;

    channel->eventfd = -1;
    shm_offset += ri_channel_shm_size(channel);
  }

  r = ri_vector_set_info(vec, &vxfer->info);
  if (r < 0)
    goto fail_channel;

  return vec;

fail_channel:
fail_shm:
  ri_vector_delete(vec);
fail_alloc:
  return NULL;
}


unsigned ri_vector_num_producers(const ri_vector_t *vec)
{
  return vec->n_producers;
}


unsigned ri_vector_num_consumers(const ri_vector_t *vec)
{
  return vec->n_consumers;
}


int ri_vector_set_info(ri_vector_t* vec, const ri_info_t *info)
{
  if (info->size == 0 || !vec->info.data)
    return 0;

  vec->info.data = malloc(info->size);

  if (!vec->info.data)
    return -ENOMEM;

  memcpy(vec->info.data, info->data, info->size);

  vec->info.size = info->size;

  return 0;
}


ri_info_t ri_vector_get_info(const ri_vector_t* vec)
{
  return (ri_info_t) {
    .data = vec->info.data,
    .size = vec->info.size,
  };
}


void ri_vector_free_info(ri_vector_t* vec)
{
  if (vec->info.data) {
    free(vec->info.data);
    vec->info.data = NULL;
    vec->info.size = 0;
  }
}

const ri_producer_t* ri_vector_get_producer(const ri_vector_t *vec, unsigned index)
{
  if (index >= vec->n_producers)
    return NULL;

  return vec->producers[index];
}


const ri_consumer_t* ri_vector_get_consumer(const ri_vector_t *vec, unsigned index)
{
  if (index >= vec->n_consumers)
    return NULL;

  return vec->consumers[index];
}

ri_producer_t* ri_vector_take_producer(ri_vector_t *vec, unsigned index)
{
  if (index >= vec->n_producers)
    return NULL;

  ri_producer_t* producer = vec->producers[index];

  vec->producers[index] = NULL;

  return producer;
}


ri_consumer_t* ri_vector_take_consumer(ri_vector_t *vec, unsigned index)
{
  if (index >= vec->n_consumers)
    return NULL;

  ri_consumer_t* consumer = vec->consumers[index];

  vec->consumers[index] = NULL;

  return consumer;
}


int ri_vector_get_shmfd(const ri_vector_t* vec)
{
  return ri_shm_get_fd(vec->shm);
}






