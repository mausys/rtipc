#include "vector.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "rtipc.h"
#include "channel.h"

struct ri_vector {
  unsigned n_consumers;
  unsigned n_producers;
  ri_consumer_t **consumers;
  ri_producer_t **producers;
  struct {
    size_t size;
    void *data;
  } info;
};


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

  free(vec);
}

static int init_producers(ri_vector_t *vec, ri_resource_t *rsc, ri_shm_t *shm, size_t *shm_offset)
{
  for (unsigned i = 0; i < vec->n_producers; i++) {
    ri_channel_t *channel = &rsc->producers[i];

    vec->producers[i] = ri_producer_new(channel, shm, *shm_offset);

    if (!vec->producers[i])
      return -1;

    *shm_offset += ri_channel_shm_size(channel);
  }

  return 0;
}


static int init_consumers(ri_vector_t *vec, ri_resource_t *rsc, ri_shm_t *shm, size_t *shm_offset)
{
  for (unsigned i = 0; i < vec->n_consumers; i++) {
    ri_channel_t *channel = &rsc->consumers[i];

    vec->consumers[i] = ri_consumer_new(channel, shm, *shm_offset);

    if (!vec->consumers[i])
      return -1;

    *shm_offset += ri_channel_shm_size(channel);
  }

  return 0;
}


ri_vector_t* ri_vector_new(ri_resource_t *rsc, bool server)
{
  unsigned n_consumers = ri_count_channels(rsc->consumers);
  unsigned n_producers = ri_count_channels(rsc->producers);

  ri_vector_t *vec = ri_vector_alloc(n_consumers, n_producers);

  if (!vec)
    goto fail_alloc;

  ri_shm_t *shm = ri_shm_map(rsc->shmfd);

  if (!shm)
    goto fail_shm;

  size_t shm_offset = 0;

  if (server) {
    int r = init_consumers(vec, rsc, shm, &shm_offset);

    if (r < 0)
      goto fail_channel;

    r = init_producers(vec, rsc, shm, &shm_offset);

    if (r < 0)
      goto fail_channel;
  } else {
    int r = init_producers(vec, rsc, shm, &shm_offset);

    if (r < 0)
      goto fail_channel;

    r = init_consumers(vec, rsc, shm, &shm_offset);

    if (r < 0)
      goto fail_channel;
  }

  int r = ri_vector_set_info(vec, &rsc->info);
    if (r < 0)
      goto fail_channel;

  ri_shm_unref(shm);
  return vec;

fail_channel:
  ri_shm_unref(shm);
fail_shm:
  ri_vector_delete(vec);
fail_alloc:
  return NULL;
}


void ri_vector_init_shm(const ri_vector_t *vec)
{
  for (unsigned i = 0; i < vec->n_consumers; i++) {
    ri_consumer_init_shm(vec->consumers[i]);
  }

  for (unsigned i = 0; i < vec->n_producers; i++) {
    ri_producer_init_shm(vec->producers[i]);
  }
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






