#pragma once

#include "rtipc.h"
#include "shm.h"

typedef struct ri_consumer_queue ri_consumer_queue_t;

ri_consumer_queue_t* ri_consumer_queue_new(const ri_channel_param_t *param, ri_shm_t *shm, size_t shm_offset);
void ri_consumer_queue_delete(ri_consumer_queue_t *consumer);
