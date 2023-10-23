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

#define RI_NUM_BUFFERS 3

typedef enum {
    RI_BUFFER_0 = 0,
    RI_BUFFER_1,
    RI_BUFFER_2,
    RI_BUFFER_NONE,
} ri_buffer_t;

typedef struct ri_chnmap {
    ri_xchg_t *xchg;
    void *bufs[RI_NUM_BUFFERS];
} ri_chnmap_t;

typedef struct ri_tchn {
    ri_chnmap_t map;
    ri_buffer_t current;
    ri_buffer_t locked;
} ri_tchn_t;

typedef struct ri_rchn {
    ri_chnmap_t map;
} ri_rchn_t;


ri_chnmap_t ri_chnmap(ri_xchg_t *xchg, void *p, size_t buf_size);

void ri_rchn_init(ri_rchn_t *chn, const ri_chnmap_t *map);

void ri_tchn_init(ri_tchn_t *chn, const ri_chnmap_t *map);

void* ri_rchn_fetch(ri_rchn_t *chn);

void* ri_tchn_swap(ri_tchn_t *chn);

bool ri_tchn_ackd(const ri_tchn_t *chn);


size_t ri_chn_buf_size(const ri_chnmap_t *map);

size_t ri_tchn_buf_size(const ri_tchn_t *chn);

size_t ri_rchn_buf_size(const ri_rchn_t *chn);


static inline size_t ri_chn_calc_size(size_t buf_size)
{
    return RI_NUM_BUFFERS * buf_size;
}


static inline size_t ri_chn_size(const ri_chnmap_t *map)
{
    return RI_NUM_BUFFERS * ri_chn_buf_size(map);
}
