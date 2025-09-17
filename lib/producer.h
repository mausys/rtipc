#pragma once

#include <stddef.h>
#include <stdint.h>


#include "rtipc.h"
#include "shm.h"


typedef struct ri_producer_queue ri_producer_queue_t;


ri_producer_queue_t * ri_producer_queue_new(ri_shm_t *shm, const ri_channel_param_t *param, uintptr_t start, bool shm_init);
void ri_producer_queue_delete(ri_producer_queue_t* producer);

void* ri_producer_queue_msg(ri_producer_queue_t *producer);
