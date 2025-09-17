#include "header.h"

#include <stdint.h>
#include <errno.h>

#include "index.h"
#include "log.h"
#include "mem_utils.h"


#define MAGIC 0x1f0c /* lock-free and zero-copy :) */
#define HEADER_VERSION 1


typedef struct request_header
{
  uint16_t magic;
  uint16_t version;
  uint16_t cacheline_size;
  uint16_t atomic_size;
} request_header_t;

size_t ri_request_header_size()
{
  return (sizeof(request_header_t));
}

int ri_request_header_validate(const void *request)
{
  const request_header_t *header = request;
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

void ri_request_header_write(void *request)
{
  request_header_t *header = request;

  header->magic = MAGIC;
  header->version = HEADER_VERSION;
  header->cacheline_size = cacheline_size();
  header->atomic_size = sizeof(ri_atomic_index_t);
}
