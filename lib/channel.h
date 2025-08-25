#pragma once

#include <stddef.h>
#include <stdint.h>


#include "rtipc.h"

#include "index.h"

#define RI_CHANNEL_MIN_MSGS 3

typedef struct ri_channel {
    /* number of messages available, can't be lesser than 3 */
    unsigned n_msgs;
    size_t msg_size;
    uintptr_t msgs_start_addr;
    /* producer and consumer can change the tail
    *  the MSB shows who has last modified the tail */
    ri_atomic_index_t *tail;
    /* head is only written by producer and only used
    * in consumer_get_head */
    ri_atomic_index_t *head;
    /* circular queue for ordering the messages,
    * initialized simple as queue[i] = (i + 1) % n,
    * but due to overruns might get scrambled.
    * only producer can modify the queue */
    ri_atomic_index_t *queue;
} ri_channel_t;


static inline ri_index_t  ri_channel_get_next(const ri_channel_t *channel, ri_index_t current)
{
    return atomic_load(&channel->queue[current]);
}

static inline void* ri_channel_get_msg(const ri_channel_t *channel, ri_index_t index)
{
    if (index >= channel->n_msgs) {
        return NULL;
    }

    return (void*)(channel->msgs_start_addr + (index * channel->msg_size));
}

size_t ri_channel_calc_size(const ri_channel_param_t *size);

uintptr_t ri_channel_init(ri_channel_t *channel, uintptr_t start, const ri_channel_param_t *size);

void ri_channel_shm_init(ri_channel_t *channel);

void ri_channel_dump(ri_channel_t *channel);
