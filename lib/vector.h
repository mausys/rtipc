#pragma once

#include "rtipc.h"

void ri_vector_delete(ri_vector_t* vec);

ri_info_t ri_vector_get_info(const ri_vector_t* vec);

void ri_vector_free_info(ri_vector_t* vec);
