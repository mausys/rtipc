#include "param.h"

#include <stdlib.h>
#include <unistd.h>

#include "index.h"
#include "mem_utils.h"


ri_vector_transfer_t* ri_vector_transfer_new(unsigned n_consumers, unsigned n_producers, const ri_info_t *info)
{
  ri_vector_transfer_t *vxfer = malloc(sizeof(ri_vector_transfer_t));

  if (!vxfer)
    goto fail_alloc;

  /* consumers and producers are terminated list add 2 elemets for termination */
  ri_channel_t *channels = calloc(n_consumers + n_producers + 2, sizeof(ri_channel_t));

  if (!channels) {
    goto fail_channels;
  }


  *vxfer = (ri_vector_transfer_t) {
    .consumers = channels,
    .producers = &channels[n_consumers + 1],
    .info = *info,
    .shmfd = -1,
  };

  for (ri_channel_t *channel = vxfer->consumers; channel->msg_size != 0; channel++)
    channel->eventfd = -1;

  for (ri_channel_t *channel = vxfer->producers; channel->msg_size != 0; channel++)
    channel->eventfd = -1;

  return vxfer;

fail_channels:
  free(vxfer);
fail_alloc:
  return NULL;
}


void ri_vector_transfer_delete(ri_vector_transfer_t *vxfer)
{
  if (vxfer->shmfd > 0)
    close(vxfer->shmfd);

  for (ri_channel_t *channel = vxfer->consumers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      close(channel->eventfd);
    }
  }

  for (ri_channel_t *channel = vxfer->producers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      close(channel->eventfd);
    }
  }

  if (vxfer->consumers)
    free(vxfer->consumers);

  free(vxfer);
}


static size_t ri_calc_data_size(unsigned n_msgs, size_t msg_size)
{
  return n_msgs * cacheline_aligned(msg_size);
}


size_t ri_calc_queue_size(unsigned n_msgs)
{
  unsigned  n = n_msgs + 2; /* tail + head*/


  return cacheline_aligned(n * sizeof(ri_atomic_index_t));
}

size_t ri_calc_channel_shm_size(unsigned n_msgs, size_t msg_size)
{
  /* tail + head + queue*/
  return ri_calc_queue_size(n_msgs) + ri_calc_data_size(n_msgs, msg_size);
}
