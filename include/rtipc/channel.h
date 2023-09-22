#pragma once

#include <stdbool.h>
#include <stdatomic.h>
#include <stddef.h>

#if ATOMIC_INT_LOCK_FREE == 2
typedef atomic_uint ri_xchg_t;
#elif ATOMIC_SHORT_LOCK_FREE == 2
typedef atomic_ushort ri_xchg_t;
#elif ATOMIC_CHAR_LOCK_FREE == 2
typedef atomic_uchar ri_xchg_t;
#else
#warning "no suitable always lockfree datatype found"
typedef atomic_uint ri_xchg_t;
#endif


typedef enum {
    RI_BUFFER_0 = 0,
    RI_BUFFER_1,
    RI_BUFFER_2,
    RI_BUFFER_NONE,
} ri_buffer_t;

typedef struct {
    ri_xchg_t *xchg;
    void *bufs[3];
} ri_chnmap_t;

typedef struct {
    ri_chnmap_t map;
    ri_buffer_t current;
    ri_buffer_t locked;
} ri_tchn_t;

typedef struct {
    ri_chnmap_t map;
} ri_rchn_t;


void* ri_tchn_init(ri_tchn_t *chn);

void* ri_rchn_update(ri_rchn_t *chn);

void* ri_tchn_update(ri_tchn_t *chn);

bool ri_tchn_ackd(const ri_tchn_t *chn);

size_t ri_chn_size(const ri_chnmap_t *map);

size_t ri_tchn_size(const ri_tchn_t *chn);

size_t ri_rchn_size(const ri_rchn_t *chn);
