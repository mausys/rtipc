#pragma once

#include "rtipc.h"

struct ri_vector {
  unsigned n_consumers;
  unsigned n_producers;
  ri_consumer_t **consumers;
  ri_producer_t **producers;
  struct {
    size_t size;
    void *data;
  } info;
};

void ri_vector_delete(ri_vector_t* vec);

int ri_vector_set_info(ri_vector_t* vec, const ri_info_t *info);

ri_info_t ri_vector_get_info(const ri_vector_t* vec);

void ri_vector_free_info(ri_vector_t* vec);
