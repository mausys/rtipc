#pragma once

#include <stdbool.h>

#include <stddef.h>


#ifdef __cplusplus

#include <atomic>

extern "C" {

#if ATOMIC_INT_LOCK_FREE == 2
typedef std::atomic_uint ri_xchg_t;
#elif ATOMIC_SHORT_LOCK_FREE == 2
typedef std::atomic_ushort ri_xchg_t;
#elif ATOMIC_CHAR_LOCK_FREE == 2
typedef std::atomic_uchar ri_xchg_t;
#else
#warning "no suitable always lockfree datatype found"
typedef std::atomic_uint ri_xchg_t;
#endif

#else

#include <stdatomic.h>

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

#endif

#define RI_NUM_BUFFERS 3

typedef enum {
    RI_BUFIDX_0 = 0,
    RI_BUFIDX_1,
    RI_BUFIDX_2,
    RI_BUFIDX_NONE,
} ri_bufidx_t;

typedef struct ri_channel {
    ri_xchg_t *xchg;
    void *bufs[RI_NUM_BUFFERS];
} ri_channel_t;

typedef struct ri_producer {
    ri_channel_t chn;
    ri_bufidx_t current;
    ri_bufidx_t locked;
} ri_producer_t;

typedef struct ri_consumer {
    ri_channel_t chn;
} ri_consumer_t;


ri_channel_t ri_channel_create(ri_xchg_t *xchg, void *p, size_t buf_size);

void ri_consumer_init(ri_consumer_t *cns, const ri_channel_t *chn);

void ri_producer_init(ri_producer_t *prd, const ri_channel_t *chn);

void* ri_consumer_fetch(ri_consumer_t *cns);

void* ri_producer_swap(ri_producer_t *prd);

bool ri_producer_ackd(const ri_producer_t *prd);


size_t ri_channel_get_buffer_size(const ri_channel_t *chn);



static inline size_t ri_calc_channel_size(size_t buf_size)
{
    return RI_NUM_BUFFERS * buf_size;
}


static inline size_t ri_get_channel_size(const ri_channel_t *chn)
{
    return RI_NUM_BUFFERS * ri_channel_get_buffer_size(chn);
}

#ifdef __cplusplus
}
#endif
