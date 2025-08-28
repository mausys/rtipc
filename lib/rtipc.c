#include "rtipc_private.h"

#include <errno.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "consumer.h"
#include "index.h"
#include "log.h"
#include "mem_utils.h"
#include "producer.h"
#include "shm.h"

#define MAGIC 0x1f0c /* lock-free and zero-copy :) */
#define HEADER_VERSION 1

struct ri_rtipc
{
  ri_shm_t *shm;
  uint32_t num_consumers;
  uint32_t num_producers;
  ri_consumer_t *consumers;
  ri_producer_t *producers;
};

typedef struct
{
  uint16_t magic;
  uint16_t version;
  uint32_t cookie;          /**< cookie for object protocol */
  uint32_t num_channels[2]; /**< number of channels for producers / consumers */
  uint16_t cacheline_size;
  uint16_t atomic_size;
} shm_header_t;

typedef uintptr_t (*init_channel_fn)(ri_rtipc_t *rtipc,
                                     unsigned idx,
                                     uintptr_t start,
                                     const ri_channel_param_t *size);

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

static uintptr_t init_consumer(ri_rtipc_t *rtipc,
                               unsigned idx,
                               uintptr_t start,
                               const ri_channel_param_t *size)
{
  ri_consumer_t *consumer = &rtipc->consumers[idx];

  return ri_consumer_init(consumer, start, size);
}

static uintptr_t init_producer(ri_rtipc_t *rtipc,
                               unsigned idx,
                               uintptr_t start,
                               const ri_channel_param_t *size)
{
  ri_producer_t *producer = &rtipc->producers[idx];

  return ri_producer_init(producer, start, size);
}

static size_t get_channels_offset(unsigned num)
{
  size_t size = header_size();
  size += num * sizeof(ri_channel_param_t); // table size
  return cacheline_aligned(size);
}

static int validate_header(const shm_header_t *header, uint32_t cookie)
{
  if (header->magic != MAGIC) {
    LOG_ERR("magic missmatch 0x%x != 0x%x", header->magic, MAGIC);
    return -EINVAL;
  }

  if (header->version != HEADER_VERSION) {
    LOG_ERR("verison missmatch %u != %u", header->version, HEADER_VERSION);
    return -EINVAL;
  }

  if (header->cookie != cookie) {
    LOG_ERR("cookie missmatch %x != %x", header->cookie, cookie);
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
                        uint32_t num_producers,
                        uint32_t cookie)
{
  header->magic = MAGIC;
  header->version = HEADER_VERSION;
  header->cookie = cookie;
  header->cacheline_size = cacheline_size();
  header->atomic_size = sizeof(ri_atomic_index_t);
  header->num_channels[0] = num_consumers;
  header->num_channels[1] = num_producers;
}

size_t ri_calc_shm_size(const ri_channel_param_t consumers[], const ri_channel_param_t producers[])
{
  unsigned num_consumers = count_channels(consumers);
  unsigned num_producers = count_channels(producers);

  size_t size = get_channels_offset(num_consumers + num_producers);

  for (unsigned i = 0; i < num_consumers; i++)
    size += ri_channel_calc_size(&consumers[i]);

  for (unsigned i = 0; i < num_producers; i++)
    size += ri_channel_calc_size(&producers[i]);

  return size;
}

ri_rtipc_t* ri_rtipc_new(ri_shm_t *shm, uint32_t cookie)
{
  ri_rtipc_t *rtipc = calloc(1, sizeof(ri_rtipc_t));

  if (!rtipc)
    goto fail_alloc;

  rtipc->shm = shm;

  if (shm->size < header_size())
    goto fail_valid;

  const shm_header_t *header = shm->mem;

  if (validate_header(header, cookie) < 0)
    goto fail_valid;

  unsigned num_g0 = header->num_channels[0];
  unsigned num_g1 = header->num_channels[1];
  unsigned num_channels = num_g0 + num_g1;

  rtipc->num_consumers = shm->owner ? num_g0 : num_g1;
  rtipc->num_producers = shm->owner ? num_g1 : num_g0;
  init_channel_fn init_channel_g0 = shm->owner ? init_consumer : init_producer;
  init_channel_fn init_channel_g1 = shm->owner ? init_producer : init_consumer;

  if (rtipc->num_consumers > 0) {
    rtipc->consumers = calloc(rtipc->num_consumers, sizeof(ri_consumer_t));

    if (!rtipc->consumers)
      goto fail_alloc_consumers;
  }

  if (rtipc->num_producers > 0) {
    rtipc->producers = calloc(rtipc->num_producers, sizeof(ri_producer_t));

    if (!rtipc->producers)
      goto fail_alloc_producers;
  }

  uintptr_t addr = (uintptr_t)shm->mem + get_channels_offset(num_channels);
  uintptr_t addr_end = (uintptr_t)shm->mem + shm->size;

  /* check if table doesn't exeeds shm size */
  if (addr > addr_end)
    goto fail_size;

  for (unsigned i = 0; i < num_g0; i++) {
    const ri_channel_param_t *channel_size = shm_get_channel_size(shm->mem, i);

    addr = init_channel_g0(rtipc, i, addr, channel_size);
  }

  for (unsigned i = 0; i < num_g1; i++) {
    const ri_channel_param_t *channel_size = shm_get_channel_size(shm->mem, num_g0 + i);

    addr = init_channel_g1(rtipc, i, addr, channel_size);
  }

  /* check if data doesn't exeeds shm size */
  if (addr > addr_end)
    goto fail_size;

  for (unsigned i = 0; i < rtipc->num_producers; i++) {
    int r = ri_producer_alloc_queue(&rtipc->producers[i]);
    if (r < 0)
      goto fail_queues;
  }

  return rtipc;

fail_queues:
  for (unsigned i = 0; i < rtipc->num_producers; i++)
    ri_producer_free_queue(&rtipc->producers[i]);

fail_size:
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
                               const ri_channel_param_t producers[],
                               uint32_t cookie)
{
  if (shm->size <= header_size())
    return NULL;

  uint32_t num_consumers = count_channels(consumers);
  uint32_t num_producers = count_channels(producers);

  shm_header_t *header = shm->mem;

  init_header(header, num_consumers, num_producers, cookie);

  ri_channel_param_t *channel_table = mem_offset(shm->mem, header_size());

  for (unsigned i = 0; i < num_consumers; i++)
    channel_table[i] = consumers[i];

  for (unsigned i = 0; i < num_producers; i++)
    channel_table[num_consumers + i] = producers[i];

  ri_rtipc_t *rtipc = ri_rtipc_new(shm, cookie);

  if (!rtipc)
    return NULL;

  for (unsigned i = 0; i < num_consumers; i++)
    ri_channel_shm_init(&rtipc->consumers[i].channel);

  for (unsigned i = 0; i < num_producers; i++)
    ri_channel_shm_init(&rtipc->producers[i].channel);

  return rtipc;
}

void ri_rtipc_delete(ri_rtipc_t *rtipc)
{
  for (unsigned i = 0; i < rtipc->num_producers; i++)
    ri_producer_free_queue(&rtipc->producers[i]);

  if (rtipc->producers)
    free(rtipc->producers);

  if (rtipc->consumers)
    free(rtipc->consumers);

  ri_shm_delete(rtipc->shm);

  free(rtipc);
}

ri_consumer_t* ri_rtipc_get_consumer(const ri_rtipc_t *rtipc, unsigned idx)
{
  if (idx >= rtipc->num_consumers)
    return NULL;

  return &rtipc->consumers[idx];
}

ri_producer_t* ri_rtipc_get_producer(const ri_rtipc_t *rtipc, unsigned idx)
{
  if (idx >= rtipc->num_producers)
    return NULL;

  return &rtipc->producers[idx];
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

  for (unsigned i = 0; i < rtipc->num_consumers; i++) {
    ri_channel_dump(&rtipc->consumers[i].channel);
  }

  LOG_INF("\tproducers (%u):", rtipc->num_producers);

  for (unsigned i = 0; i < rtipc->num_producers; i++) {
    ri_channel_dump(&rtipc->producers[i].channel);
  }
}
