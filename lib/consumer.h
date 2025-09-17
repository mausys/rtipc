#pragma once

#include "rtipc.h"
#include "shm.h"

typedef struct ri_consumer_queue ri_consumer_queue_t;

ri_consumer_queue_t* ri_consumer_queue_new(ri_shm_t *shm, const ri_channel_param_t *param, uintptr_t start, bool shm_init);
void ri_consumer_queue_delete(ri_consumer_queue_t *consumer);
