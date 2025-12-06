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




static ri_channel_param_t entry_to_param(const entry_t *entry, const ri_info_t *info)
{
  return (ri_channel_param_t) {
      .add_msgs = entry->add_msgs,
      .msg_size = entry->msg_size,
      .info = *info,
      .eventfd = entry->eventfd,
  };
}



static ri_channel_param_t producer_to_param(const ri_producer_t *producer)
{
  return (ri_channel_param_t) {
    .add_msgs = ri_producer_len(producer) - RI_CHANNEL_MIN_MSGS,
    .msg_size = ri_producer_msg_size(producer),
    .info = ri_producer_info(producer),
    .eventfd = ri_producer_eventfd(producer) > 0 ? 1 : 0,
  };
}


static ri_channel_param_t consumer_to_param(const ri_consumer_t *consumer)
{
  return (ri_channel_param_t) {
      .add_msgs = ri_consumer_len(consumer) - RI_CHANNEL_MIN_MSGS,
      .msg_size = ri_consumer_msg_size(consumer),
      .info = ri_consumer_info(consumer),
      .eventfd = ri_consumer_eventfd(consumer) > 0 ? 1 : 0,
  };
}

static int req_write_param(ri_uxmsg_t *req, const ri_channel_param_t *param, size_t *entry_offset, size_t *info_offset)
{
  entry_t entry = {
      .add_msgs = param->add_msgs,
      .msg_size = param->msg_size,
      .info_size = param->info.size,
      .eventfd = param->eventfd,
  };

  int r = ri_uxmsg_write(req, entry_offset, &entry, sizeof(entry));

  if (r < 0)
    return r;

  if (param->info.data) {
    r = ri_uxmsg_write(req, info_offset, param->info.data, param->info.size);

    if (r < 0)
      return r;
  }

  return r;
}

static int req_read_param(ri_uxmsg_t *req, ri_channel_param_t *param, size_t *entry_offset, size_t *info_offset)
{
  entry_t entry;
  int r = ri_uxmsg_read(req, entry_offset, &entry, sizeof(entry));

  if (r < 0)
    return -r;

  ri_info_t info = { .size = entry.info_size };

  if ( info.size > 0) {
    info.data = ri_uxmsg_ptr(req, *info_offset, info.size);

    if (!info.data)
      return -1;

    *info_offset += info.size;
  }

  *param =  entry_to_param(&entry, &info);

  return r;
}

ri_vector_t* ri_request_parse(ri_uxmsg_t *req)
{
  size_t msg_size =  ri_uxmsg_size(req);

  const void* ptr = ri_uxmsg_ptr(req, 0, ri_request_header_size());

  if (!ptr) {
    LOG_ERR("request too small (%zu) for header", msg_size);
    goto fail_verify;
  }

  int r = ri_request_header_validate(ptr);

  if (r < 0) {
    LOG_ERR("ri_request_header_validate failed");
    goto fail_verify;
  }

  size_t offset = ri_request_header_size();


  uint32_t vec_info_size;
  r = ri_uxmsg_read(req, &offset, &vec_info_size, sizeof(vec_info_size));

  if (r < 0) {
    LOG_ERR("request too small (%zu) for vec_info_size", msg_size);
    goto fail_verify;
  }


  uint32_t num_consumers;
  r = ri_uxmsg_read(req, &offset, &num_consumers, sizeof(num_consumers));

  if (r < 0) {
    LOG_ERR("request too small (%zu) for num_consumers", msg_size);
    goto fail_verify;
  }

  uint32_t num_producers;
  r = ri_uxmsg_read(req, &offset, &num_producers, sizeof(num_producers));

  if (r < 0) {
    LOG_ERR("request too small (%zu) for num_producers", msg_size);
    goto fail_verify;
  }

  unsigned num_channels = num_producers + num_consumers;

  size_t info_offset = offset + num_channels * sizeof(entry_t);

  int memfd = ri_uxmsg_pop_fd(req);

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
      .size = vec_info_size,
      .data = ri_uxmsg_ptr(req, info_offset, vec_info_size),
    };

    if (!vec_info.data) {
      LOG_ERR("messsage too small (%zu) for vector info", msg_size);
      goto fail_verify;
    }

    int r = ri_vector_set_info(vec, &vec_info);

    if (r < 0)
      goto fail_alloc;

    info_offset += vec_info_size;
  }

  vec->shm = ri_shm_map(memfd);

  if (!vec->shm)
    goto fail_shm;

  size_t shm_offset = 0;

  for (unsigned i = 0; i < num_consumers; i++) {
    ri_channel_param_t param;
    int r = req_read_param(req, &param, &offset, &info_offset);

    if (r < 0)
      goto fail_channel;

    int fd = - 1;

    if (param.eventfd) {
      fd = ri_uxmsg_pop_fd(req);

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

    shm_offset += ri_param_channel_shm_size(&param);
  }

  for (unsigned i = 0; i < num_producers; i++) {
    ri_channel_param_t param;
    int r = req_read_param(req, &param, &offset, &info_offset);

    if (r < 0)
      goto fail_channel;

    int fd = - 1;

    if (param.eventfd) {
      fd = ri_uxmsg_pop_fd(req);

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


ri_uxmsg_t* ri_request_create(const ri_vector_t* vec)
{
  size_t msg_size = calc_msg_size(vec);

  ri_uxmsg_t *req = ri_uxmsg_new(msg_size);

  if (!req)
    goto fail_alloc;

  void *ptr = ri_uxmsg_ptr(req, 0, ri_request_header_size());

  ri_request_header_write(ptr);

  size_t offset = ri_request_header_size();

  unsigned num_channels = vec->num_producers + vec->num_consumers;

  ri_info_t vector_info = ri_vector_get_info(vec);

  ri_uxmsg_write(req, &offset, &vector_info.size, sizeof(uint32_t));

  ri_uxmsg_write(req, &offset, &vec->num_producers, sizeof(uint32_t));

  ri_uxmsg_write(req, &offset, &vec->num_consumers, sizeof(uint32_t));

  size_t info_offset = offset + num_channels * sizeof(entry_t);

  if (vector_info.data)
    ri_uxmsg_write(req, &info_offset, vector_info.data, vector_info.size);

  ri_uxmsg_push_fd(req, ri_shm_fd(vec->shm));

  for (unsigned i = 0 ; i < vec->num_producers; i++) {
    const ri_producer_t *producer = vec->producers[i];
    ri_channel_param_t param = producer_to_param(producer);

    int eventfd =  ri_producer_eventfd(producer);

    req_write_param(req, &param, &offset, &info_offset);

    if ( eventfd > 0)
      ri_uxmsg_push_fd(req, eventfd);
  }

  for (unsigned i = 0 ; i < vec->num_consumers; i++) {
    const ri_consumer_t *consumer = vec->consumers[i];
    ri_channel_param_t param = consumer_to_param(consumer);

    int eventfd =  ri_consumer_eventfd(consumer);

    req_write_param(req, &param, &offset, &info_offset);

    if ( eventfd > 0)
      ri_uxmsg_push_fd(req, eventfd);
  }

  return req;

fail_alloc:
  return NULL;
}

