#pragma once

#include <stddef.h>
#include <stdint.h>


#include "rtipc.h"
#include "shm.h"


typedef struct ri_producer_q ri_producer_q_t;


ri_producer_q_t * ri_producer_q_new(ri_shm_t *shm, const ri_channel_param_t *param, uintptr_t start, bool shm_init);
void ri_producer_q_delete(ri_producer_q_t* producer);

void* ri_producer_q_msg(ri_producer_q_t *producer);
