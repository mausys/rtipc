#include "header.h"

#include <stdint.h>
#include <errno.h>
#include <string.h>


#include "index.h"
#include "log.h"
#include "mem_utils.h"


#define MAGIC 0x1f0c /* lock-free and zero-copy :) */
#define HEADER_VERSION 1


int ri_request_header_validate(const ri_request_header_t *header)
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


ri_request_header_t ri_request_header_init(void)
{
  return (ri_request_header_t) {
    .magic = MAGIC,
    .version = HEADER_VERSION,
    .cacheline_size = cacheline_size(),
    .atomic_size = sizeof(ri_atomic_index_t),
  };
}
