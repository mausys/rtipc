#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "rtipc.h"
#include "mem_utils.h"
#include "unix.h"
#include "channel.h"
#include "request.h"

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
      rsc->consumers[i].eventfd = ri_eventfd_create();
      if (rsc->consumers[i].eventfd < 0)
        goto fail_init;
    }
  }

  for (unsigned i = 0; i < n_producers; i++) {
    rsc->producers[i] = config->producers[i];

    if (config->producers[i].eventfd > 0) {
      rsc->producers[i].eventfd = ri_eventfd_create();
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
  if (rsc->shmfd >= 0)
    close(rsc->shmfd);

  for (ri_channel_t *channel = rsc->consumers; channel->msg_size != 0; channel++) {
    if (channel->eventfd >= 0) {
      close(channel->eventfd);
    }
  }

  for (ri_channel_t *channel = rsc->producers; channel->msg_size != 0; channel++) {
    if (channel->eventfd >= 0) {
      close(channel->eventfd);
    }
  }

  if (rsc->consumers)
    free(rsc->consumers);

  free(rsc);
}

static int get_fd(unsigned idx, const int fds[], unsigned n_fds)
{
  if (idx >= n_fds)
    return -1;

  return fds[idx];
}

size_t ri_resource_serialize_size(const ri_resource_t *rsc)
{
  return ri_request_calc_size(rsc);
}

int ri_resource_serialize(const ri_resource_t *rsc, void* req, size_t size, int fds[], unsigned *n_fds)
{
  if (!n_fds || (*n_fds < 1))
    return -EINVAL;

  int r = ri_request_write(rsc, req, size);
  if (r < 0)
    return r;

  unsigned idx = 0;

  fds[idx++] = rsc->shmfd;

  for (const ri_channel_t *channel = rsc->producers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      if (idx >= *n_fds)
        return -1;

      fds[idx++] = channel->eventfd;
    }
  }

  for (const ri_channel_t *channel = rsc->consumers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      if (idx >= *n_fds)
        return -1;

      fds[idx++] = channel->eventfd;
    }
  }

  *n_fds = idx;
  return r;
}


ri_resource_t* ri_resource_deserialize(const void* req, size_t size, int fds[], unsigned *n_fds)
{
  ri_resource_t* rsc = ri_request_parse(req, size);
  if (!rsc)
    goto fail_parse;

  unsigned idx = 0;

  rsc->shmfd = get_fd(idx++, fds, *n_fds);
  if (rsc->shmfd < 0)
    goto fail_eventfd;

  for (ri_channel_t *channel = rsc->consumers; channel->msg_size != 0; channel++) {
    if (channel->eventfd <= 0)
      continue;

    channel->eventfd = get_fd(idx++, fds, *n_fds);

    if (channel->eventfd < 0)
      goto fail_eventfd;
  }

  for (ri_channel_t *channel = rsc->producers; channel->msg_size != 0; channel++) {
    if (channel->eventfd <= 0)
      continue;

    channel->eventfd = get_fd(idx++, fds, *n_fds);

    if (channel->eventfd < 0)
      goto fail_eventfd;
  }

  /* transfer ownership to resource and close unused fds */
  for (unsigned i = 0; i < *n_fds; i++) {
    if (fds[i] < 0)
      continue;

    if (i >= idx) {
      close(fds[i]);
    }

    fds[i] = -1;
  }

  *n_fds = idx;

  return rsc;

fail_eventfd:
  if (rsc->consumers)
    free(rsc->consumers);

  free(rsc);
fail_parse:
  return NULL;
}

