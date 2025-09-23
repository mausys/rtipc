#pragma once

#include "rtipc.h"

#include "shm.h"


ri_consumer_t* ri_consumer_new(const ri_channel_param_t *param, ri_shm_t *shm, size_t shm_offset, int fd, bool shm_init);
ri_producer_t* ri_producer_new(const ri_channel_param_t *param, ri_shm_t *shm, size_t shm_offset, int fd, bool shm_init);

void ri_consumer_delete(ri_consumer_t *consumer);
void ri_producer_delete(ri_producer_t *producer);

size_t ri_consumer_shm_offset(const ri_consumer_t *consumer);

unsigned ri_consumer_len(const ri_consumer_t *consumer);

size_t ri_consumer_msg_size(const ri_consumer_t *consumer);

ri_info_t ri_consumer_info(const ri_consumer_t *consumer);

int ri_consumer_eventfd(const ri_consumer_t *consumer);

size_t ri_prdoucer_shm_offset(const ri_producer_t *producer);

unsigned ri_producer_len(const ri_producer_t *producer);

size_t ri_producer_msg_size(const ri_producer_t *producer);

ri_info_t ri_producer_info(const ri_producer_t *producer);

int ri_producer_eventfd(const ri_producer_t *producer);
