#pragma once

#include <stdint.h>

typedef struct ri_request_header {
  uint16_t magic;
  uint16_t version;
  uint16_t cacheline_size;
  uint16_t atomic_size;
} ri_request_header_t;

int ri_request_header_validate(const ri_request_header_t *header);
ri_request_header_t ri_request_header_init(void);
