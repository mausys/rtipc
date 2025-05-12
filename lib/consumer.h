#pragma once

#include "channel.h"

typedef struct ri_consumer {
    ri_channel_t channel;
    ri_index_t current;
} ri_consumer_t;


uintptr_t ri_consumer_init(ri_consumer_t *consumer, uintptr_t start, const ri_channel_size_t *size);

