#pragma once

#include "rtipc.h"

void ri_vector_delete(ri_vector_t* vec);

int ri_vector_set_info(ri_vector_t* vec, const ri_info_t *info);

ri_info_t ri_vector_get_info(const ri_vector_t* vec);

void ri_vector_free_info(ri_vector_t* vec);
