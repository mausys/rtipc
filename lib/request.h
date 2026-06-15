#pragma once

#include "rtipc.h"

size_t ri_request_calc_size(const ri_config_t *config);

ri_config_t ri_request_parse(const void *req, size_t size, ri_channel_t **rsc);

int ri_request_write(const ri_config_t* config, void *req, size_t size);
