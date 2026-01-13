#pragma once

#include <stddef.h>
#include <stdint.h>


#include "rtipc.h"
#include "shm.h"


typedef struct ri_producer_queue ri_producer_queue_t;

ri_producer_queue_t * ri_producer_queue_new(const ri_channel_t *channel, ri_shm_t *shm, size_t shm_offset);

void ri_producer_queue_init_shm(const ri_producer_queue_t *producer);

void ri_producer_queue_delete(ri_producer_queue_t* producer);

unsigned ri_producer_queue_len(const ri_producer_queue_t *producer);

size_t ri_producer_queue_msg_size(const ri_producer_queue_t *producer);

void* ri_producer_queue_msg(const ri_producer_queue_t *producer);

ri_produce_result_t ri_producer_queue_force_push(ri_producer_queue_t *producer);

ri_produce_result_t ri_producer_queue_try_push(ri_producer_queue_t *producer);

bool ri_producer_queue_full(const ri_producer_queue_t *producer);
