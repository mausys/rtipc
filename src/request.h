#pragma once

#include "rtipc/rtipc.h"

size_t ri_request_calc_size(const ri_config_t *config);

ri_config_t ri_request_parse(const void *req, size_t size, ri_attr_t **attrs);

int ri_request_write(const ri_config_t* config, void *req, size_t size);
