#pragma once

#include "param.h"
#include "shm.h"

ri_consumer_t* ri_consumer_new(const ri_channel_param_t *param, ri_shm_t *shm, uintptr_t start, int fd);
ri_producer_t* ri_producer_new(const ri_channel_param_t *param, ri_shm_t *shm, uintptr_t start, int fd);

void ri_consumer_delete(ri_consumer_t *consumer);
void ri_producer_delete(ri_producer_t *producer);
