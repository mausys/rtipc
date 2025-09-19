#include "vector.h"

#include <stdalign.h>
#include <unistd.h>

#include "rtipc.h"
#include "mem_utils.h"
#include "shm.h"
#include "fd.h"
#include "header.h"
#include "channel.h"
#include "request.h"
#include "event.h"

typedef struct ri_channel_vector {
  ri_shm_t *shm;
  unsigned num_consumers;
  unsigned num_producers;
  ri_consumer_t **consumers;
  ri_producer_t **producers;
} ri_channel_vector_t;

typedef struct entry {
  uint32_t add_msgs;
  uint32_t msg_size;
  uint32_t eventfd;
  uint32_t info_size;
} entry_t;


typedef struct req_iter {
  const entry_t *entry;
  size_t shm_offset;
  size_t info_offset;
  unsigned fd_idx;
} req_iter_t;


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


static ri_channel_vector_t* vector_alloc(unsigned num_consumers, unsigned num_producers)
{
  ri_channel_vector_t *vec = malloc(sizeof(ri_channel_vector_t));

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


int check_iter(const req_iter_t *iter, const ri_request_t *req, const ri_shm_t *shm)
{
  if (iter->shm_offset >= ri_shm_size(shm))
    return -1;

  if (iter->info_offset >= ri_request_size(req))
    return -1;

  return 0;
}

ri_channel_param_t to_param(const entry_t *entry, const ri_request_t *req, size_t info_offset)
{
  return (ri_channel_param_t) {
      .add_msgs = entry->add_msgs,
      .msg_size = entry->msg_size,
      .info.size = entry->info_size,
      .info.data = cmem_offset(ri_request_msg(req), info_offset),
      .eventfd = entry->eventfd,
  };
}


static void req_iter_next(req_iter_t *iter)
{
  unsigned n_msgs = RI_CHANNEL_MIN_MSGS + iter->entry->add_msgs;

  iter->info_offset += iter->entry->info_size;
  iter->shm_offset += ri_calc_channel_size(n_msgs, iter->entry->msg_size);

  if (iter->entry->eventfd) {
    iter->fd_idx++;
  }

  iter->entry++;
}

ri_channel_vector_t * ri_channel_vector_new(const ri_channel_param_t consumers[],
                                           const ri_channel_param_t producers[])
{
  unsigned num_consumers = count_channels(consumers);
  unsigned num_producers = count_channels(producers);

  ri_channel_vector_t *vec = vector_alloc(num_consumers, num_producers);

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


ri_channel_vector_t * ri_channel_vector_from_request(ri_request_t *req)
{
  const void *msg = ri_request_msg(req);
  size_t msg_size =  ri_request_size(req);

  if (ri_request_header_size() + 2 * sizeof(uint32_t) > msg_size)
    goto fail_verify;

  if (ri_request_header_validate(msg) < 0)
    goto fail_verify;

  size_t offset = ri_request_header_size();
  offset = mem_align(offset, alignof(uint32_t));

  unsigned num_producers  = *(const uint32_t*)cmem_offset(msg, offset);
  offset += sizeof(uint32_t);

  unsigned num_consumers = *(const uint32_t*)cmem_offset(msg, offset);
  offset += sizeof(uint32_t);

  unsigned num_channels = num_producers + num_consumers;

  offset = mem_align(offset, alignof(entry_t));
  const entry_t *entry = cmem_offset(msg, offset);

  size_t info_offset = offset + num_channels * sizeof(entry_t);

  if (info_offset > msg_size)
    goto fail_verify;

  int memfd = ri_request_take_fd(req, 0);

  if (ri_fd_check(memfd, RI_FD_MEM) < 0)
    goto fail_verify;

  ri_channel_vector_t *vec = vector_alloc(num_consumers, num_producers);

  if (!vec)
    goto fail_alloc;

  ri_shm_t *shm = ri_shm_map(memfd);

  if (!shm)
    goto fail_shm;

  req_iter_t iter = {
      .info_offset = info_offset,
      .fd_idx = 1,
      .entry = entry,
   };

  for (unsigned i = 0; i < num_consumers; i++) {
    req_iter_next(&iter);

    int r = check_iter(&iter, req, shm);

    if (r < 0)
      goto fail_channel;

    ri_channel_param_t param = to_param(iter.entry, req, iter.info_offset);

    int fd = - 1;

    if (param.eventfd) {
      fd = ri_request_take_fd(req, iter.fd_idx);

      if (fd >= 0) {
        if (ri_fd_check(fd, RI_FD_EVENT) < 0) {
          close(fd);
          goto fail_channel;
        }
      }
    }

    vec->consumers[i] = ri_consumer_new(&param, shm, iter.shm_offset, fd);

    if (!vec->consumers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);
      goto fail_channel;
    }
  }

  for (unsigned i = 0; i < num_producers; i++) {
    req_iter_next(&iter);

    int r = check_iter(&iter, req, shm);

    if (r < 0)
      goto fail_channel;

    ri_channel_param_t param = to_param(iter.entry, req, iter.info_offset);

    int fd = - 1;

    if (param.eventfd) {
      fd = ri_request_take_fd(req, iter.fd_idx);

      if (fd >= 0) {
        if (ri_fd_check(fd, RI_FD_EVENT) < 0) {
          close(fd);
          goto fail_channel;
        }
      }
    }

    vec->producers[i] = ri_producer_new(&param, shm, iter.shm_offset, fd);

    if (!vec->producers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);
      goto fail_channel;
    }
  }

  return vec;

fail_channel:
  ri_shm_delete(shm);
fail_shm:
  ri_channel_vector_delete(vec);
fail_alloc:
fail_verify:
  return NULL;
}


