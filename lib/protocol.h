#pragma once

#include "rtipc.h"

size_t ri_request_calc_size(const ri_resource_t *rsc);

ri_resource_t* ri_request_parse(const void *req, size_t size);

int ri_request_write(const ri_resource_t* rsc, void *req, size_t size);
