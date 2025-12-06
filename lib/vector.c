#include "vector.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "rtipc.h"
#include "param.h"
#include "channel.h"
#include "fd.h"
#include "unix_message.h"
#include "protocol.h"

static unsigned count_channels(const ri_channel_param_t channels[])
{
  if (!channels)
    return 0;

  unsigned i;

  for (i = 0; channels[i].msg_size != 0; i++)
    ;

  return i;
}


static size_t calc_shm_size(const ri_channel_param_t consumers[], const ri_channel_param_t producers[])
{
  unsigned num_consumers = count_channels(consumers);
  unsigned num_producers = count_channels(producers);

  size_t size = 0;

  for (unsigned i = 0; i < num_consumers; i++)
    size +=  ri_param_channel_shm_size(&consumers[i]);

  for (unsigned i = 0; i < num_producers; i++)
    size += ri_param_channel_shm_size(&producers[i]);

  return size;
}


ri_vector_t* ri_vector_alloc(unsigned num_consumers, unsigned num_producers)
{
  ri_vector_t *vec = calloc(1, sizeof(ri_vector_t));

  if (!vec)
    goto fail_alloc;

  if (num_consumers > 0) {
    vec->consumers = calloc(num_consumers, sizeof(ri_consumer_t*));

    if (!vec->consumers)
      goto fail_consumers;
  }

  if (num_producers > 0) {
    vec->producers = calloc(num_producers, sizeof(ri_producer_t*));

    if (!vec->producers)
      goto fail_producers;
  }

  vec->num_consumers = num_consumers;
  vec->num_producers = num_producers;

  return vec;

fail_producers:
  if (num_consumers > 0)
    free(vec->consumers);
fail_consumers:
  free(vec);
fail_alloc:
  return NULL;
}



void ri_vector_delete(ri_vector_t* vec)
{
  if (vec->consumers) {
    for (unsigned i = 0; i < vec->num_consumers; i++) {
      if (vec->consumers[i]) {
        ri_consumer_delete(vec->consumers[i]);
      }
    }
    free(vec->consumers);
  }

  if (vec->producers) {
    for (unsigned i = 0; i < vec->num_producers; i++) {
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



ri_vector_t* ri_vector_new( const ri_channel_param_t producers[], const ri_channel_param_t consumers[],
                                          const ri_info_t *info)
{
  unsigned num_consumers = count_channels(consumers);
  unsigned num_producers = count_channels(producers);

  ri_vector_t *vec = ri_vector_alloc(num_consumers, num_producers);

  if (!vec)
    goto fail_alloc;

  size_t shm_size = calc_shm_size(consumers, producers);

  vec->shm = ri_shm_new(shm_size);

  if (!vec->shm)
    goto fail_shm;

  size_t shm_offset = 0;

  for (unsigned i = 0; i < num_producers; i++) {
    const ri_channel_param_t *param = &producers[i];
    int fd = - 1;

    if (param->eventfd) {
      fd = ri_eventfd();

      if (fd < 0)
        goto fail_channel;
    }

    vec->producers[i] = ri_producer_new(param, vec->shm, shm_offset, fd, true);

    if (!vec->producers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);

      goto fail_channel;
    }

    shm_offset += ri_param_channel_shm_size(param);
  }


  for (unsigned i = 0; i < num_consumers; i++) {
    const ri_channel_param_t *param = &consumers[i];
    int fd = - 1;

    if (param->eventfd) {
      fd = ri_eventfd();

      if (fd < 0)
          goto fail_channel;
    }

    vec->consumers[i] = ri_consumer_new(param, vec->shm, shm_offset, fd, true);

    if (!vec->consumers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);

      goto fail_channel;
    }


    shm_offset += ri_param_channel_shm_size(param);
  }

  if (info && info->data) {
    int r = ri_vector_set_info(vec, info);
    if (r < 0)
      goto fail_channel;
  }

  return vec;

fail_channel:
fail_shm:
  ri_vector_delete(vec);
fail_alloc:
  return NULL;
}


int ri_vector_set_info(ri_vector_t* vec, const ri_info_t *info)
{
  if (info->size == 0)
    return -EINVAL;

  if (vec->info.data) {
    return -EEXIST;
  }

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



ri_producer_t* ri_vector_take_producer(ri_vector_t *vec, unsigned index)
{
  if (index >= vec->num_producers)
    return NULL;

  ri_producer_t* producer = vec->producers[index];

  vec->producers[index] = NULL;

  return producer;
}


ri_consumer_t* ri_vector_take_consumer(ri_vector_t *vec, unsigned index)
{
  if (index >= vec->num_consumers)
    return NULL;

  ri_consumer_t* consumer = vec->consumers[index];

  vec->consumers[index] = NULL;

  return consumer;
}





