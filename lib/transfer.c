
#include <stdlib.h>
#include <unistd.h>


#include "rtipc.h"
#include "unix.h"
#include "channel.h"
#include "index.h"
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


ri_transfer_t* ri_transfer_alloc(unsigned n_consumers, unsigned n_producers, const ri_info_t *info)
{
  ri_transfer_t *xfer = malloc(sizeof(ri_transfer_t));

  if (!xfer)
    goto fail_alloc;

  /* consumers and producers are terminated list add 2 elemets for termination */
  ri_channel_t *channels = calloc(n_consumers + n_producers + 2, sizeof(ri_channel_t));

  if (!channels) {
    goto fail_channels;
  }


  *xfer = (ri_transfer_t) {
    .consumers = channels,
    .producers = &channels[n_consumers + 1],
    .info = *info,
    .shmfd = -1,
  };

  for (ri_channel_t *channel = xfer->consumers; channel->msg_size != 0; channel++)
    channel->eventfd = -1;

  for (ri_channel_t *channel = xfer->producers; channel->msg_size != 0; channel++)
    channel->eventfd = -1;

  return xfer;

fail_channels:
  free(xfer);
fail_alloc:
  return NULL;
}


ri_transfer_t* ri_transfer_new(const ri_config_t *config)
{
  unsigned n_consumers = ri_count_channels(config->consumers);
  unsigned n_producers = ri_count_channels(config->producers);

  ri_transfer_t *xfer = ri_transfer_alloc(n_consumers, n_producers, &config->info);

  if (!xfer)
    goto fail_alloc;

  size_t shm_size = ri_calc_shm_size(config->consumers, config->producers);

  xfer->shmfd = ri_shmfd_create(shm_size);

  if (xfer->shmfd < 0)
    goto fail_init;

  for (unsigned i = 0; i < n_consumers; i++) {
    xfer->consumers[i] = config->consumers[i];

    if (config->consumers[i].eventfd > 0) {
      xfer->consumers[i].eventfd = ri_eventfd();
      if (xfer->consumers[i].eventfd < 0)
        goto fail_init;
    }
  }

  for (unsigned i = 0; i < n_producers; i++) {
    xfer->producers[i] = config->producers[i];

    if (config->producers[i].eventfd > 0) {
      xfer->producers[i].eventfd = ri_eventfd();
      if (xfer->producers[i].eventfd < 0)
        goto fail_init;
    }
  }

  return xfer;

fail_init:
  ri_transfer_delete(xfer);
fail_alloc:
  return NULL;
}


void ri_transfer_delete(ri_transfer_t *xfer)
{
  if (xfer->shmfd > 0)
    close(xfer->shmfd);

  for (ri_channel_t *channel = xfer->consumers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      close(channel->eventfd);
    }
  }

  for (ri_channel_t *channel = xfer->producers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      close(channel->eventfd);
    }
  }

  if (xfer->consumers)
    free(xfer->consumers);

  free(xfer);
}


