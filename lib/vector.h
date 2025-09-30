#pragma once

#include "rtipc.h"
#include "shm.h"

struct ri_vector {
  unsigned num_consumers;
  unsigned num_producers;
  ri_consumer_t **consumers;
  ri_producer_t **producers;
  struct {
    size_t size;
    void *data;
  } info;
  ri_shm_t *shm;
};


ri_vector_t* ri_vector_alloc(unsigned num_consumers, unsigned num_producers);

ri_vector_t* ri_vector_new( const ri_channel_param_t producers[], const ri_channel_param_t consumers[],
                           const ri_info_t *info);

void ri_vector_delete(ri_vector_t* vec);

int ri_vector_set_info(ri_vector_t* vec, const ri_info_t *info);

ri_info_t ri_vector_get_info(const ri_vector_t* vec);

void ri_vector_free_info(ri_vector_t* vec);
