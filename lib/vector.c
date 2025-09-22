#include "vector.h"

#include <stdlib.h>
#include <unistd.h>

#include "rtipc.h"
#include "param.h"
#include "shm.h"
#include "channel.h"
#include "event.h"



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
    size +=  ri_param_channel_size(&consumers[i]);

  for (unsigned i = 0; i < num_producers; i++)
    size += ri_param_channel_size(&producers[i]);

  return size;
}


ri_channel_vector_t* ri_channel_vector_alloc(unsigned num_consumers, unsigned num_producers)
{
  ri_channel_vector_t *vec = calloc(1, sizeof(ri_channel_vector_t));

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



void ri_channel_vector_delete(ri_channel_vector_t* vec)
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

  free(vec);
}



ri_channel_vector_t * ri_channel_vector_new(const ri_channel_param_t consumers[],
                                           const ri_channel_param_t producers[])
{
  unsigned num_consumers = count_channels(consumers);
  unsigned num_producers = count_channels(producers);

  ri_channel_vector_t *vec = ri_channel_vector_alloc(num_consumers, num_producers);

  if (!vec)
    goto fail_alloc;

  size_t shm_size = calc_shm_size(consumers, producers);

  ri_shm_t* shm = ri_shm_new(shm_size);

  if (!shm)
    goto fail_shm;

  size_t shm_offset = 0;

  for (unsigned i = 0; i < num_consumers; i++) {
    const ri_channel_param_t *param = &consumers[i];
    int fd = - 1;

    if (param->eventfd) {
      fd = ri_event_create();

      if (fd < 0)
          goto fail_channel;
    }

    vec->consumers[i] = ri_consumer_new(param, shm, shm_offset, fd);

    if (!vec->consumers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);

      goto fail_channel;
    }

    shm_offset += ri_param_channel_size(param);
  }

  for (unsigned i = 0; i < num_producers; i++) {
    const ri_channel_param_t *param = &producers[i];
    int fd = - 1;

    if (param->eventfd) {
      fd = ri_event_create();

      if (fd < 0)
        goto fail_channel;
    }

    vec->producers[i] = ri_producer_new(param, shm, shm_offset, fd);

    if (!vec->producers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);

      goto fail_channel;
    }

    shm_offset += ri_param_channel_size(param);
  }

  return vec;

fail_channel:
  ri_shm_delete(shm);
fail_shm:
  ri_channel_vector_delete(vec);
fail_alloc:
  return NULL;
}


