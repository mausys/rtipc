#pragma once

#include <limits.h>
#include <stdatomic.h>
#include <stddef.h>

#if ATOMIC_INT_LOCK_FREE == 2

typedef unsigned int ri_index_t;
typedef atomic_uint ri_atomic_index_t;

#define RI_INDEX_INVALID UINT_MAX

#define RI_CONSUMED_FLAG ((ri_index_t)(UINT_MAX - UINT_MAX / 2))

#define RI_FIRST_FLAG ((ri_index_t)(RI_CONSUMED_FLAG >> 1))

#define RI_ORIGIN_MASK RI_CONSUMED_FLAG

#define RI_INDEX_MASK (~(RI_ORIGIN_MASK | RI_FIRST_FLAG))

#else

#error "atomic_uint not available"

#endif
