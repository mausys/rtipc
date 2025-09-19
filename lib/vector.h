#pragma once

#include "rtipc.h"
#include "request.h"

typedef struct ri_channel_vector ri_channel_vector_t;

ri_channel_vector_t* ri_channel_vector_from_request(ri_request_t *req);
