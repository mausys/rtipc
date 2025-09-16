#pragma once

#include <stddef.h>
#include <stdint.h>


#include "rtipc.h"
#include "shm.h"


typedef struct ri_producerq ri_producerq_t;


ri_producerq_t * ri_producerq_new(ri_shm_t *shm, const ri_channel_param_t *param, uintptr_t start, bool shm_init);
void ri_producerq_delete(ri_producerq_t* producer);

void* ri_producerq_msg(ri_producerq_t *producer);
