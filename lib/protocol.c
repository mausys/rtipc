#include "protocol.h"

#include <stdalign.h>
#include <unistd.h>
#include <string.h>

#include "mem_utils.h"
#include "param.h"
#include "shm.h"
#include "channel.h"
#include "header.h"
#include "fd.h"

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



static size_t calc_msg_size(const ri_channel_vector_t *vec)
{
  size_t size = ri_request_header_size();
  size = mem_align(size, alignof(uint32_t));
  /* number of channels */
  size +=  2 * sizeof(uint32_t);
  size = mem_align(size, alignof(entry_t));

  /* channel table */
  size += (vec->num_consumers + vec->num_producers) * sizeof(entry_t);

  /* channel info */
  for (unsigned i = 0; i < vec->num_consumers; i++)
    size +=  ri_consumer_info(vec->consumers[i]).size;

  for (unsigned i = 0; i < vec->num_producers; i++)
    size += ri_producer_info(vec->producers[i]).size;

  return size;
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


ri_channel_vector_t * ri_channel_vector_from_request(ri_request_t *req)
{
  const void *msg = ri_request_msg(req);
  size_t msg_size =  ri_request_size(req);

  size_t offset = ri_request_header_size();
  offset = mem_align(offset, alignof(uint32_t));

  if (offset + 2 * sizeof(uint32_t) > msg_size)
    goto fail_verify;

  if (ri_request_header_validate(msg) < 0)
    goto fail_verify;

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

  ri_channel_vector_t *vec = ri_channel_vector_alloc(num_consumers, num_producers);

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


ri_request_t* ri_request_from_channel_vector(const ri_channel_vector_t* vec)
{
  size_t msg_size = calc_msg_size(vec);

  ri_request_t *req = ri_request_new(msg_size);

  if (!req)
    goto fail_alloc;

  void *msg = ri_request_msg(req);
  ri_request_header_write(msg);

  size_t offset = mem_align(ri_request_header_size(), alignof(uint32_t));

  *(uint32_t*)mem_offset(msg, offset) = vec->num_producers;
  offset += sizeof(uint32_t);

  *(uint32_t*)mem_offset(msg, offset) = vec->num_consumers;
  offset += sizeof(uint32_t);

  offset = mem_align(offset, alignof(entry_t));
  entry_t *entry = mem_offset(msg, offset);

  unsigned num_channels = vec->num_producers + vec->num_consumers;

  size_t info_offset = offset + num_channels * sizeof(entry_t);

  for (unsigned i = 0 ; i < vec->num_producers; i++) {
    const ri_producer_t *producer = vec->producers[i];

     int eventfd =  ri_producer_eventfd(producer);

    ri_info_t info = ri_producer_info(producer);

    *entry = (entry_t) {
      .add_msgs = ri_producer_len(producer) - RI_CHANNEL_MIN_MSGS,
      .msg_size = ri_producer_msg_size(producer),
      .info_size = info.size,
      .eventfd = eventfd > 0 ? 1 : 0,
    };

    if ( eventfd > 0) {
      ri_request_add_fd(req, eventfd);
    }

    if (info.size > 0) {
      memcpy(mem_offset(msg, info_offset), info.data, info.size);
      info_offset += info.size;
    }
  }

  for (unsigned i = 0 ; i < vec->num_consumers; i++) {
    const ri_consumer_t *consumer = vec->consumers[i];

    int eventfd =  ri_consumer_eventfd(consumer);

    ri_info_t info = ri_consumer_info(consumer);

    *entry = (entry_t) {
        .add_msgs = ri_consumer_len(consumer) - RI_CHANNEL_MIN_MSGS,
        .msg_size = ri_consumer_msg_size(consumer),
        .info_size = info.size,
        .eventfd =  ri_consumer_eventfd(consumer) >= 0 ? 1 : 0,
    };

    if ( eventfd > 0) {
      ri_request_add_fd(req, eventfd);
    }

    if (info.size > 0) {
      memcpy(mem_offset(msg, info_offset), info.data, info.size);
      info_offset += info.size;
    }
  }

  return req;

fail_alloc:
  return NULL;
}

