#pragma once

#include "unix_message.h"
#include "vector.h"

ri_vector_t* ri_request_parse(ri_uxmsg_t *req);
ri_uxmsg_t* ri_request_create(const ri_vector_t* vec);
