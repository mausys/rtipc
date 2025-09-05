#pragma once

#include <stddef.h>
#include <stdint.h>


#include "rtipc.h"
#include "shm.h"


typedef struct ri_producer ri_producer_t;


ri_producer_t * ri_producer_new(ri_shm_t *shm, const ri_channel_param_t *param, uintptr_t start, bool shm_init);
void ri_producer_delete(ri_producer_t* producer);

void* ri_producer_msg(ri_producer_t *producer);
