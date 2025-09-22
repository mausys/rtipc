#pragma once

#include <stddef.h>
#include <stdint.h>


#include "rtipc.h"
#include "shm.h"


typedef struct ri_producer_queue ri_producer_queue_t;


unsigned ri_producer_queue_len(const ri_producer_queue_t *producer);
size_t ri_producer_queue_msg_size(const ri_producer_queue_t *producer);

ri_producer_queue_t * ri_producer_queue_new(const ri_channel_param_t *param, ri_shm_t *shm, size_t shm_offset);
void ri_producer_queue_delete(ri_producer_queue_t* producer);

void* ri_producer_queue_msg(ri_producer_queue_t *producer);
