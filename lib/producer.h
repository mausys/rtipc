#pragma once

#include <stddef.h>
#include <stdint.h>

#include "channel.h"

typedef struct ri_producer {
    ri_channel_t channel;
    ri_index_t head; /* last message in chain that can be used by consumer, chain[head] is always INDEX_END */
    ri_index_t current; /* message used by producer, will become head  */
    ri_index_t overrun; /* message used by consumer when tail moved away by producer, will become current when released by consumer */
} ri_producer_t;


uintptr_t ri_producer_init(ri_producer_t *producer, uintptr_t start, const ri_channel_param_t *size);

void* ri_producer_msg(ri_producer_t *producer);
