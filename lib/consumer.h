#pragma once

#include "rtipc.h"
#include "shm.h"

typedef struct ri_consumer ri_consumer_t;

ri_consumer_t* ri_consumer_new(ri_shm_t *shm, const ri_channel_param_t *param, uintptr_t start, bool shm_init);
void ri_consumer_delete(ri_consumer_t *consumer);
