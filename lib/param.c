#include "param.h"

#include <stdlib.h>
#include <unistd.h>

#include "index.h"
#include "mem_utils.h"


ri_vector_map_t* ri_vector_map_new(unsigned n_consumers, unsigned n_producers, const ri_info_t *info)
{
  ri_vector_map_t *vmap = malloc(sizeof(ri_vector_map_t));

  if (!vmap)
    goto fail_alloc;

  /* consumers and producers are terminated list add 2 elemets for termination */
  ri_channel_param_t *params = calloc(n_consumers + n_producers + 2, sizeof(ri_channel_param_t));

  if (!params) {
    goto fail_params;
  }


  *vmap = (ri_vector_map_t) {
    .consumers = params,
    .producers = &params[n_consumers + 1],
    .info = *info,
    .shmfd = -1,
  };

  for (ri_channel_param_t *param = vmap->consumers; param->msg_size != 0; param++)
    param->eventfd = -1;

  for (ri_channel_param_t *param = vmap->producers; param->msg_size != 0; param++)
    param->eventfd = -1;

  return vmap;

fail_params:
  free(vmap);
fail_alloc:
  return NULL;
}


void ri_vector_map_delete(ri_vector_map_t *vmap)
{
  if (vmap->shmfd > 0)
    close(vmap->shmfd);

  for (ri_channel_param_t *param = vmap->consumers; param->msg_size != 0; param++) {
    if (param->eventfd > 0) {
      close(param->eventfd);
    }
  }

  for (ri_channel_param_t *param = vmap->producers; param->msg_size != 0; param++) {
    if (param->eventfd > 0) {
      close(param->eventfd);
    }
  }

  if (vmap->consumers)
    free(vmap->consumers);

  free(vmap);
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
