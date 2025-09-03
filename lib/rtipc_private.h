#pragma once

#include "rtipc.h"
#include "shm.h"

ri_rtipc_t* ri_rtipc_new(ri_shm_t *shm, uint32_t cookie, bool shm_init);
ri_rtipc_t* ri_rtipc_owner_new(ri_shm_t *shm, const ri_channel_param_t consumers[], const ri_channel_param_t producers[], uint32_t cookie);
