#pragma once

#include "rtipc.h"

typedef struct ri_odb ri_odb_t;
typedef struct ri_odb_entry ri_odb_entry_t;
typedef struct ri_odb_group ri_odb_group_t;

typedef struct ri_odb_iter {
    const ri_odb_group_t *group;
    const ri_odb_entry_t *entry;
    unsigned chn_idx;
} ri_odb_iter_t;


typedef void* (*ri_odb_map_producer_fn) (ri_producer_mapper_t *producer, uint64_t id, const ri_object_t *object, void *user_data);
typedef void* (*ri_odb_map_consumer_fn) (ri_consumer_mapper_t *consumer, uint64_t id, const ri_object_t *object, void *user_data);
typedef void* (*ri_odb_unmap_fn) (uint64_t id, const ri_object_t *object, void *user_data);


void ri_odb_delete(ri_odb_t *odb);

ri_odb_t *ri_odb_new(unsigned max_consumer_channels, unsigned max_producer_channels);

int ri_odb_add_consumer_object(ri_odb_t *odb, uint64_t id, const ri_object_t *object, ri_odb_map_consumer_fn map_cb, ri_odb_unmap_fn unmap_cb, void *user_data);

int ri_odb_add_producer_object(ri_odb_t *odb, uint64_t id, const ri_object_t *object, ri_odb_map_producer_fn map_cb, ri_odb_unmap_fn unmap_cb, void *user_data);

int ri_odb_consumer_channel_add_object(ri_odb_t *odb, unsigned chn_id, uint64_t obj_id, const ri_object_t *object, ri_odb_map_consumer_fn map_cb, ri_odb_unmap_fn unmap_cb, void *user_data);

int ri_odb_producer_channel_add_object(ri_odb_t *odb, unsigned chn_id, uint64_t obj_id, const ri_object_t *object, ri_odb_map_producer_fn map_cb, ri_odb_unmap_fn unmap_cb, void *user_data);

int ri_odb_assign_producer_object(ri_odb_t *odb, uint64_t obj_id, unsigned chn_id);

int ri_odb_assign_consumer_object(ri_odb_t *odb, uint64_t obj_id, unsigned chn_id);

ri_odb_iter_t ri_odb_consumer_iter_begin(ri_odb_t *odb);

ri_odb_iter_t ri_odb_producer_iter_begin(ri_odb_t *odb);

int ri_odb_iter_next(ri_odb_iter_t *iter);

bool ri_odb_iter_end(const ri_odb_iter_t *iter);

const ri_object_t* ri_odb_iter_get(const ri_odb_iter_t *iter, uint64_t *id, unsigned *chn_idx);
