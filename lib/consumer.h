#pragma once

#include "rtipc.h"
#include "shm.h"

typedef struct ri_consumerq ri_consumerq_t;

ri_consumerq_t* ri_consumerq_new(ri_shm_t *shm, const ri_channel_param_t *param, uintptr_t start, bool shm_init);
void ri_consumerq_delete(ri_consumerq_t *consumer);
