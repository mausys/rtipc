#pragma once

#include "rtipc.h"

typedef struct ri_channel_vector {
  unsigned num_consumers;
  unsigned num_producers;
  ri_consumer_t **consumers;
  ri_producer_t **producers;
} ri_channel_vector_t;


ri_channel_vector_t* ri_channel_vector_alloc(unsigned num_consumers, unsigned num_producers);
void ri_channel_vector_delete(ri_channel_vector_t* vec);
