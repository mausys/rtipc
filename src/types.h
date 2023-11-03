#pragma once

#include <stdbool.h>
#include <stddef.h>
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


typedef struct ri_shm {
    void *p;
    size_t size;
    int fd;
    char *path;
    bool owner;
    struct {
        ri_consumer_t *list;
        unsigned num;
    } consumers;
    struct {
        ri_producer_t *list;
        unsigned num;
    } producers;
} ri_shm_t;
