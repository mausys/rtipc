
#include <stdlib.h>
#include <unistd.h>


#include "rtipc.h"
#include "unix.h"
#include "channel.h"
#include "mem_utils.h"

static size_t ri_calc_data_size(unsigned n_msgs, size_t msg_size)
{
  return n_msgs * cacheline_aligned(msg_size);
}


size_t ri_calc_channel_shm_size(unsigned n_msgs, size_t msg_size)
{
  /* tail + head + queue*/
  return ri_calc_queue_size(n_msgs) + ri_calc_data_size(n_msgs, msg_size);
}


ri_resource_t* ri_resource_new(unsigned n_consumers, unsigned n_producers, const ri_info_t *info)
{
  ri_resource_t *rsc = malloc(sizeof(ri_resource_t));

  if (!rsc)
    goto fail_alloc;

  /* consumers and producers are terminated list add 2 elemets for termination */
  ri_channel_t *channels = calloc(n_consumers + n_producers + 2, sizeof(ri_channel_t));

  if (!channels) {
    goto fail_channels;
  }


  *rsc = (ri_resource_t) {
    .consumers = channels,
    .producers = &channels[n_consumers + 1],
    .info = *info,
    .shmfd = -1,
  };

  for (ri_channel_t *channel = rsc->consumers; channel->msg_size != 0; channel++)
    channel->eventfd = -1;

  for (ri_channel_t *channel = rsc->producers; channel->msg_size != 0; channel++)
    channel->eventfd = -1;

  return rsc;

fail_channels:
  free(rsc);
fail_alloc:
  return NULL;
}


ri_resource_t* ri_resource_alloc(const ri_config_t *config)
{
  unsigned n_consumers = ri_count_channels(config->consumers);
  unsigned n_producers = ri_count_channels(config->producers);

  ri_resource_t *rsc = ri_resource_new(n_consumers, n_producers, &config->info);

  if (!rsc)
    goto fail_alloc;

  size_t shm_size = ri_calc_shm_size(config->consumers, config->producers);

  rsc->shmfd = ri_shmfd_create(shm_size);

  if (rsc->shmfd < 0)
    goto fail_init;

  for (unsigned i = 0; i < n_consumers; i++) {
    rsc->consumers[i] = config->consumers[i];

    if (config->consumers[i].eventfd > 0) {
      rsc->consumers[i].eventfd = ri_eventfd();
      if (rsc->consumers[i].eventfd < 0)
        goto fail_init;
    }
  }

  for (unsigned i = 0; i < n_producers; i++) {
    rsc->producers[i] = config->producers[i];

    if (config->producers[i].eventfd > 0) {
      rsc->producers[i].eventfd = ri_eventfd();
      if (rsc->producers[i].eventfd < 0)
        goto fail_init;
    }
  }

  return rsc;

fail_init:
  ri_resource_delete(rsc);
fail_alloc:
  return NULL;
}


void ri_resource_delete(ri_resource_t *rsc)
{
  if (rsc->shmfd > 0)
    close(rsc->shmfd);

  for (ri_channel_t *channel = rsc->consumers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      close(channel->eventfd);
    }
  }

  for (ri_channel_t *channel = rsc->producers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      close(channel->eventfd);
    }
  }

  if (rsc->consumers)
    free(rsc->consumers);

  free(rsc);
}


