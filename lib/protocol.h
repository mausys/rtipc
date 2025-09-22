#pragma once

#include "request.h"
#include "vector.h"

ri_vector_t* ri_channel_vector_from_request(ri_request_t *req);
ri_request_t* ri_request_from_channel_vector(const ri_vector_t* vec);
