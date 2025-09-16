#include "rtipc_private.h"

#include <errno.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "param.h"
#include "index.h"
#include "log.h"
#include "mem_utils.h"

#include "shm.h"
#include "consumer.h"
#include "producer.h"

#define MAGIC 0x1f0c /* lock-free and zero-copy :) */
#define HEADER_VERSION 1

struct ri_rtipc
{
  ri_shm_t *shm;
  uint32_t num_consumers;
  uint32_t num_producers;
  ri_consumerq_t **consumers;
  ri_producerq_t **producers;
};

typedef struct
{
  uint16_t magic;
  uint16_t version;
  uint16_t cacheline_size;
  uint16_t atomic_size;
  uint32_t num_channels[2]; /**< number of channels for producers / consumers */
} shm_header_t;


static size_t header_size(void)
{
  return (sizeof(shm_header_t));
}

static unsigned count_channels(const ri_channel_param_t channels[])
{
  if (!channels)
    return 0;

  unsigned i;

  for (i = 0; channels[i].msg_size != 0; i++)
    ;

  return i;
}

static const ri_channel_param_t* shm_get_channel_size(void *shm_start, unsigned idx)
{
  const ri_channel_param_t *sizes = mem_offset(shm_start, header_size());

  return &sizes[idx];
}


static size_t get_channels_offset(unsigned num)
{
  size_t size = header_size();
  size += num * sizeof(ri_channel_param_t); // table size
  return cacheline_aligned(size);
}

static int validate_header(const shm_header_t *header)
{
  if (header->magic != MAGIC) {
    LOG_ERR("magic missmatch 0x%x != 0x%x", header->magic, MAGIC);
    return -EINVAL;
  }

  if (header->version != HEADER_VERSION) {
    LOG_ERR("verison missmatch %u != %u", header->version, HEADER_VERSION);
    return -EINVAL;
  }


  if (header->cacheline_size != cacheline_size()) {
    LOG_ERR("cacheline_size missmatch %u != %zu", header->cacheline_size, cacheline_size());
    return -EINVAL;
  }

  if (header->atomic_size != sizeof(ri_atomic_index_t)) {
    LOG_ERR("atomic size missmatch %u != %zu", header->atomic_size, sizeof(ri_atomic_index_t));
    return -EINVAL;
  }

  return 0;
}

static void init_header(shm_header_t *header,
                        uint32_t num_consumers,
                        uint32_t num_producers)
{
  header->magic = MAGIC;
  header->version = HEADER_VERSION;
  header->cacheline_size = cacheline_size();
  header->atomic_size = sizeof(ri_atomic_index_t);
  header->num_channels[0] = num_consumers;
  header->num_channels[1] = num_producers;
}

static ssize_t init_producers(ri_rtipc_t *rtipc, uintptr_t addr, uintptr_t addr_end, unsigned index_offset, bool shm_init)
{
  for (unsigned i = 0; i < rtipc->num_producers; i++) {
    const ri_channel_param_t *param = shm_get_channel_size(rtipc->shm->mem, index_offset + i);

    size_t size = ri_calc_channel_size(param);
    if (addr + size > addr_end)
      return -1;

    rtipc->producers[i] = ri_producerq_new(rtipc->shm, param, addr, shm_init);
    if (!rtipc->producers[i])
      return -1;

    addr += size;
  }
  return addr;
}



static ssize_t init_consumers(ri_rtipc_t *rtipc, uintptr_t addr, uintptr_t addr_end, unsigned index_offset, bool shm_init)
{
  for (unsigned i = 0; i < rtipc->num_consumers; i++) {
    const ri_channel_param_t *param = shm_get_channel_size(rtipc->shm->mem, index_offset + i);

    size_t size = ri_calc_channel_size(param);
    if (addr + size > addr_end)
      return -1;

    rtipc->consumers[i] = ri_consumerq_new(rtipc->shm, param, addr, shm_init);
    if (!rtipc->consumers[i])
      return -1;

    addr += size;
  }

  return addr;
}

size_t ri_calc_shm_size(const ri_channel_param_t consumers[], const ri_channel_param_t producers[])
{
  unsigned num_consumers = count_channels(consumers);
  unsigned num_producers = count_channels(producers);

  size_t size = get_channels_offset(num_consumers + num_producers);

  for (unsigned i = 0; i < num_consumers; i++)
    size += ri_calc_channel_size(&consumers[i]);

  for (unsigned i = 0; i < num_producers; i++)
    size += ri_calc_channel_size(&producers[i]);

  return size;
}

ri_rtipc_t* ri_rtipc_new(ri_shm_t *shm, bool shm_init)
{
  ri_rtipc_t *rtipc = calloc(1, sizeof(ri_rtipc_t));

  if (!rtipc)
    goto fail_alloc;

  rtipc->shm = shm;

  if (shm->size < header_size())
    goto fail_valid;

  const shm_header_t *header = shm->mem;

  if (validate_header(header) < 0)
    goto fail_valid;

  unsigned num_g0 = header->num_channels[0];
  unsigned num_g1 = header->num_channels[1];
  unsigned num_channels = num_g0 + num_g1;


  rtipc->num_consumers = shm->owner ? num_g0 : num_g1;
  rtipc->num_producers = shm->owner ? num_g1 : num_g0;

  if (rtipc->num_consumers > 0) {
    rtipc->consumers = calloc(rtipc->num_consumers, sizeof(ri_consumerq_t*));

    if (!rtipc->consumers)
      goto fail_alloc_consumers;
  }

  if (rtipc->num_producers > 0) {
    rtipc->producers = calloc(rtipc->num_producers, sizeof(ri_producerq_t*));

    if (!rtipc->producers)
      goto fail_alloc_producers;
  }

  uintptr_t addr = (uintptr_t)shm->mem + get_channels_offset(num_channels);
  uintptr_t addr_end = (uintptr_t)shm->mem + shm->size;

  /* check if table doesn't exeeds shm size */
  if (addr > addr_end)
    goto fail_channels;

  if (shm->owner) {
     ssize_t ret = init_consumers(rtipc, addr, addr_end, 0, shm_init);
     if (ret < 0)
      goto fail_channels;

     addr = ret;

     ret = init_producers(rtipc, addr, addr_end, rtipc->num_consumers, shm_init);
     if (ret < 0)
       goto fail_channels;
  } else {
    ssize_t ret = init_producers(rtipc, addr, addr_end, 0, shm_init);
    if (ret < 0)
      goto fail_channels;

    addr = ret;

    ret = init_consumers(rtipc, addr, addr_end, rtipc->num_producers, shm_init);
    if (ret < 0)
      goto fail_channels;
  }


  return rtipc;

fail_channels:
  for (unsigned i = 0; i < rtipc->num_producers; i++) {
    if (rtipc->producers[i])
      ri_producerq_delete(rtipc->producers[i]);
  }

  for (unsigned i = 0; i < rtipc->num_consumers; i++) {
    if (rtipc->consumers[i])
      ri_consumerq_delete(rtipc->consumers[i]);
  }

  if (rtipc->producers)
    free(rtipc->producers);

fail_alloc_producers:
  if (rtipc->consumers)
    free(rtipc->consumers);

fail_alloc_consumers:
fail_valid:
  free(rtipc);
fail_alloc:
  return NULL;
}

ri_rtipc_t* ri_rtipc_owner_new(ri_shm_t *shm,
                               const ri_channel_param_t consumers[],
                               const ri_channel_param_t producers[])
{
  if (shm->size <= header_size())
    return NULL;

  uint32_t num_consumers = count_channels(consumers);
  uint32_t num_producers = count_channels(producers);

  shm_header_t *header = shm->mem;

  init_header(header, num_consumers, num_producers);

  ri_channel_param_t *channel_table = mem_offset(shm->mem, header_size());

  for (unsigned i = 0; i < num_consumers; i++)
    channel_table[i] = consumers[i];

  for (unsigned i = 0; i < num_producers; i++)
    channel_table[num_consumers + i] = producers[i];

  return ri_rtipc_new(shm, true);
}

void ri_rtipc_delete(ri_rtipc_t *rtipc)
{

  if (rtipc->producers) {
    for (unsigned i = 0; i < rtipc->num_producers; i++) {
      if (rtipc->producers[i])
        ri_producerq_delete(rtipc->producers[i]);
    }
    free(rtipc->producers);
  }

  if (rtipc->consumers) {
    for (unsigned i = 0; i < rtipc->num_consumers; i++) {
      if (rtipc->consumers[i])
        ri_consumerq_delete(rtipc->consumers[i]);
    }
    free(rtipc->consumers);
  }

  free(rtipc);
}

ri_consumerq_t* ri_rtipc_take_consumer(const ri_rtipc_t *rtipc, unsigned idx)
{
  if (idx >= rtipc->num_consumers)
    return NULL;

  ri_consumerq_t* consumer = rtipc->consumers[idx];
  rtipc->consumers[idx] = NULL;

  return consumer;
}

ri_producerq_t* ri_rtipc_take_producer(const ri_rtipc_t *rtipc, unsigned idx)
{
  if (idx >= rtipc->num_producers)
    return NULL;

  ri_producerq_t* producer = rtipc->producers[idx];
  rtipc->producers[idx] = NULL;

  return producer;
}

unsigned ri_rticp_num_consumers(const ri_rtipc_t *rtipc)
{
  return rtipc->num_consumers;
}

unsigned ri_rtipc_num_producers(const ri_rtipc_t *rtipc)
{
  return rtipc->num_producers;
}

int ri_rtipc_get_shm_fd(const ri_rtipc_t *rtipc)
{
  if (!rtipc->shm)
    return -1;

  return rtipc->shm->fd;
}

void ri_rtipc_dump(const ri_rtipc_t *rtipc)
{
  LOG_INF("shm addr=%p size=%zu", rtipc->shm->mem, rtipc->shm->size);
  LOG_INF("\tconsumers (%u):", rtipc->num_consumers);

  //for (unsigned i = 0; i < rtipc->num_consumers; i++) {
  //  ri_channel_dump(rtipc->consumers[i].channel);
  //}

  LOG_INF("\tproducers (%u):", rtipc->num_producers);

  //for (unsigned i = 0; i < rtipc->num_producers; i++) {
  //  ri_channel_dump(rtipc->producers[i].channel);
  //}
}
