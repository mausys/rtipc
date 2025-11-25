#include "protocol.h"

#include <stdalign.h>
#include <unistd.h>
#include <string.h>

#include "channel.h"
#include "fd.h"
#include "header.h"
#include "log.h"
#include "mem_utils.h"
#include "param.h"
#include "shm.h"

typedef struct entry {
  uint32_t add_msgs;
  uint32_t msg_size;
  int32_t eventfd;
  uint32_t info_size;
} entry_t;


typedef struct req_iter {
  const ri_request_t *req;
  entry_t entry;
  size_t info_offset;
  size_t entry_offset;
  unsigned fd_idx;
} req_iter_t;


static size_t calc_msg_size(const ri_vector_t *vec)
{
  size_t size = ri_request_header_size();

  /* vector info size + 2 * number of channels */
  size +=  3 * sizeof(uint32_t);

  /* channel table */
  size += (vec->num_consumers + vec->num_producers) * sizeof(entry_t);

  /* vector info */
  size += ri_vector_get_info(vec).size;

  /* channel info */
  for (unsigned i = 0; i < vec->num_consumers; i++)
    size +=  ri_consumer_info(vec->consumers[i]).size;

  for (unsigned i = 0; i < vec->num_producers; i++)
    size += ri_producer_info(vec->producers[i]).size;

  return size;
}




static ri_channel_param_t to_param(const entry_t *entry, const ri_request_t *req, size_t info_offset)
{
  return (ri_channel_param_t) {
      .add_msgs = entry->add_msgs,
      .msg_size = entry->msg_size,
      .info.size = entry->info_size,
      .info.data = cmem_offset(ri_request_msg(req), info_offset),
      .eventfd = entry->eventfd,
  };
}

static int req_iter_init(req_iter_t *iter, const ri_request_t *req, size_t entry_offset, size_t info_offset)
{

  *iter = (req_iter_t ) {
      .req = req,
      .entry_offset = entry_offset,
      .info_offset = info_offset,
      .fd_idx = 1,
  };

  if (iter->info_offset > ri_request_size(iter->req)) {
    LOG_ERR("info exceeds message size");
    return -1;
  }

  const void *entry_ptr = cmem_offset(ri_request_msg(iter->req), iter->entry_offset);
  memcpy(&iter->entry, entry_ptr, sizeof(entry_t));

  return 0;
}


static int req_iter_next(req_iter_t *iter)
{
  iter->entry_offset += sizeof(entry_t);
  iter->info_offset += iter->entry.info_size;

  if (iter->info_offset > ri_request_size(iter->req)) {
    LOG_ERR("info exceeds message size");
    return -1;
  }

  if (iter->entry.eventfd) {
    iter->fd_idx++;
  }


  const void *entry_ptr = cmem_offset(ri_request_msg(iter->req), iter->entry_offset);

  memcpy(&iter->entry, entry_ptr, sizeof(entry_t));

  return 0;
}


ri_vector_t* ri_vector_from_request(ri_request_t *req)
{
  const void *msg = ri_request_msg(req);
  size_t msg_size =  ri_request_size(req);

  size_t offset = ri_request_header_size();

  if (offset + 3 * sizeof(uint32_t) > msg_size) {
    LOG_ERR("messsage too small (%zu)", msg_size);
    goto fail_verify;
  }

  if (ri_request_header_validate(msg) < 0) {
    LOG_ERR("ri_request_header_validate failed");
    goto fail_verify;
  }

  uint32_t vec_info_size;
  memcpy(&vec_info_size, cmem_offset(msg, offset), sizeof(uint32_t));
  offset += sizeof(uint32_t);

  uint32_t num_consumers;
  memcpy(&num_consumers, cmem_offset(msg, offset), sizeof(uint32_t));
  offset += sizeof(uint32_t);

  uint32_t num_producers;
  memcpy(&num_producers, cmem_offset(msg, offset), sizeof(uint32_t));
  offset += sizeof(uint32_t);

  unsigned num_channels = num_producers + num_consumers;

  size_t info_offset = offset + num_channels * sizeof(entry_t);

  if (info_offset + vec_info_size > msg_size) {
     LOG_ERR("messsage too small (%zu)", msg_size);
     goto fail_verify;
  }

  int memfd = ri_request_take_fd(req, 0);

  if (ri_check_memfd(memfd) < 0) {
    if (memfd >= 0)
      close(memfd);
    LOG_ERR("memfd check failed");
    goto fail_verify;
  }

  ri_vector_t *vec = ri_vector_alloc(num_consumers, num_producers);

  if (!vec)
    goto fail_alloc;

  if (vec_info_size > 0) {
    ri_info_t vec_info = {
      .size =  vec_info_size,
      .data =  cmem_offset(msg, info_offset),
    };

    int r = ri_vector_set_info(vec, &vec_info);

    if (r < 0)
      goto fail_alloc;

    info_offset += vec_info_size;
  }

  vec->shm = ri_shm_map(memfd);

  if (!vec->shm)
    goto fail_shm;

  size_t shm_offset = 0;

  req_iter_t iter;
  int r = req_iter_init(&iter, req, offset, info_offset);

  if (r < 0)
    goto fail_channel;

  for (unsigned i = 0; i < num_consumers; i++) {
    ri_channel_param_t param = to_param(&iter.entry, req, iter.info_offset);

    int fd = - 1;

    if (param.eventfd) {
      fd = ri_request_take_fd(req, iter.fd_idx);

      if (fd >= 0) {
        if (ri_check_eventfd(fd) < 0) {
          close(fd);
          LOG_ERR("eventfd check failed");
          goto fail_channel;
        }
      }
    }

    vec->consumers[i] = ri_consumer_new(&param, vec->shm, shm_offset, fd, false);

    if (!vec->consumers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);
      goto fail_channel;
    }

    r = req_iter_next(&iter);

    if (r < 0)
      goto fail_channel;

    shm_offset += ri_param_channel_shm_size(&param);
  }

  for (unsigned i = 0; i < num_producers; i++) {
    ri_channel_param_t param = to_param(&iter.entry, req, iter.info_offset);

    int fd = - 1;

    if (param.eventfd) {
      fd = ri_request_take_fd(req, iter.fd_idx);

      if (fd >= 0) {
        if (ri_check_eventfd(fd) < 0) {
          close(fd);
          LOG_ERR("eventfd check failed");
          goto fail_channel;
        }
      }
    }

    vec->producers[i] = ri_producer_new(&param, vec->shm, shm_offset, fd, false);

    if (!vec->producers[i]) {
      /* if channel creation fails, fd has no owner */
      if (fd > 0)
        close(fd);
      goto fail_channel;
    }

    r = req_iter_next(&iter);

    if (r < 0)
      goto fail_channel;

    shm_offset += ri_param_channel_shm_size(&param);
  }

  return vec;

fail_channel:
fail_shm:
  ri_vector_delete(vec);
fail_alloc:
fail_verify:
  return NULL;
}


ri_request_t* ri_request_from_vector(const ri_vector_t* vec)
{
  size_t msg_size = calc_msg_size(vec);

  ri_request_t *req = ri_request_new(msg_size);

  if (!req)
    goto fail_alloc;

  void *msg = ri_request_msg(req);
  ri_request_header_write(msg);

  size_t offset = ri_request_header_size();

  unsigned num_channels = vec->num_producers + vec->num_consumers;

  ri_info_t vector_info = ri_vector_get_info(vec);

  memcpy(mem_offset(msg, offset), &vector_info.size, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  memcpy(mem_offset(msg, offset), &vec->num_producers, sizeof(uint32_t));
  offset += sizeof(uint32_t);

  memcpy(mem_offset(msg, offset), &vec->num_consumers, sizeof(uint32_t));
  offset += sizeof(uint32_t);

   size_t info_offset = offset + num_channels * sizeof(entry_t);

  if (vector_info.data) {
    memcpy(mem_offset(msg, info_offset), vector_info.data, vector_info.size);
    info_offset += vector_info.size;
  }

  ri_request_add_fd(req, ri_shm_fd(vec->shm));

  for (unsigned i = 0 ; i < vec->num_producers; i++) {
    const ri_producer_t *producer = vec->producers[i];

    int eventfd =  ri_producer_eventfd(producer);

    ri_info_t info = ri_producer_info(producer);

    entry_t entry = {
      .add_msgs = ri_producer_len(producer) - RI_CHANNEL_MIN_MSGS,
      .msg_size = ri_producer_msg_size(producer),
      .info_size = info.size,
      .eventfd = eventfd > 0 ? 1 : 0,
    };

    memcpy(mem_offset(msg, offset), &entry, sizeof(entry));

    if ( eventfd > 0) {
      ri_request_add_fd(req, eventfd);
    }

    if (info.size > 0) {
      memcpy(mem_offset(msg, info_offset), info.data, info.size);
      info_offset += info.size;
    }
    offset += sizeof(entry_t);
  }

  for (unsigned i = 0 ; i < vec->num_consumers; i++) {
    const ri_consumer_t *consumer = vec->consumers[i];

    int eventfd =  ri_consumer_eventfd(consumer);

    ri_info_t info = ri_consumer_info(consumer);

    entry_t entry = {
        .add_msgs = ri_consumer_len(consumer) - RI_CHANNEL_MIN_MSGS,
        .msg_size = ri_consumer_msg_size(consumer),
        .info_size = info.size,
        .eventfd =  ri_consumer_eventfd(consumer) >= 0 ? 1 : 0,
    };

    memcpy(mem_offset(msg, offset), &entry, sizeof(entry));

    if ( eventfd > 0) {
      ri_request_add_fd(req, eventfd);
    }

    if (info.size > 0) {
      memcpy(mem_offset(msg, info_offset), info.data, info.size);
      info_offset += info.size;
    }
    offset += sizeof(entry_t);
  }

  return req;

fail_alloc:
  return NULL;
}

