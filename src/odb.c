#include "odb.h"

#include <stdlib.h>
#include <errno.h>

#ifndef RI_ODB_POOL_INIT_CAP
#define RI_ODB_POOL_INIT_CAP 64
#endif

typedef void* (*ri_odb_map_producer_fn) (ri_producer_t *producer, uint64_t id, const ri_object_t *object, void *user_data);
typedef void* (*ri_odb_map_consumer_fn) (ri_consumer_t *consumer, uint64_t id, const ri_object_t *object, void *user_data);
typedef void* (*ri_odb_unmap_fn) (uint64_t id, const ri_object_t *object, void *user_data);

typedef struct ri_odb_entry ri_odb_entry_t;


struct ri_odb_entry {
    struct {
        ri_odb_entry_t *next;
        ri_odb_entry_t **prev;
    } le;
    uint64_t id;
    ri_object_t object;
    union {
        ri_odb_map_producer_fn prd_map_cb;
        ri_odb_map_consumer_fn cns_map_cb;
    };
    ri_odb_unmap_fn unmap_cb;
    void *user_data;
};


typedef struct {
    ri_odb_entry_t *first;
    ri_odb_entry_t **last;
} ri_odb_list_t;


typedef struct {
    unsigned n;							\
    ri_odb_list_t *channels;
    ri_odb_list_t unassigned;
} ri_odb_group_t;


typedef struct {
    unsigned n;
    unsigned cap;
    ri_odb_entry_t *entries;
} ri_odb_pool_t;


typedef struct {
    const ri_odb_group_t *group;
    const ri_odb_entry_t *entry;
    unsigned chn_idx;
} ri_odb_iter_t;


struct ri_odb {
    ri_odb_pool_t pool;
    ri_odb_group_t consumers;
    ri_odb_group_t producers;
};



static void ri_odb_list_remove(ri_odb_list_t *list, ri_odb_entry_t *entry)
{
    if ((entry->le.next) != NULL)
        entry->le.next->le.prev = entry->le.prev;
    else
        list->last = entry->le.prev;

    *entry->le.prev = entry->le.next;
}


static void ri_odb_list_append(ri_odb_list_t *list, ri_odb_entry_t *entry)
{
    entry->le.next = NULL;
    entry->le.prev = list->last;
    *list->last = entry;
    list->last = &entry->le.next;
}


static void ri_odb_list_move(ri_odb_entry_t *entry, ri_odb_list_t *target, ri_odb_list_t *source)
{
    ri_odb_list_remove(source, entry);
    ri_odb_list_append(target, entry);
}


static ri_odb_entry_t* ri_odb_list_get(ri_odb_list_t *list, uint64_t id)
{
    for (ri_odb_entry_t* it = list->first; it; it = it->le.next) {
        if (it->id == id)
            return it;
    }
    return NULL;
}

bool ri_odb_iter_end(const ri_odb_iter_t *iter)
{
    if (iter->entry)
        return false;

    for (unsigned chn_idx = iter->chn_idx; chn_idx < iter->group->n; chn_idx++) {
        if (!iter->group->channels[chn_idx].first)
            return false;
    }

    return true;
}



const ri_object_t* ri_odb_iter_get(const ri_odb_iter_t *iter, uint64_t *id)
{
    if (!iter->entry)
        return NULL;

    if (id)
        *id = iter->entry->id;

    return &iter->entry->object;
}


int ri_odb_iter_next(ri_odb_iter_t *iter)
{
    if (ri_odb_iter_end(iter))
        return -1;

    if (iter->entry) {
        iter->entry = iter->entry->le.next;
        if (iter->entry)
            return 0;
    }

    iter->chn_idx++;

    if (iter->chn_idx >= iter->group->n)
        return -1;

    iter->entry = iter->group->channels[iter->chn_idx].first;

    if (iter->entry)
        return 1;

    return ri_odb_iter_end(iter) ? -1 : -2;
}


static ri_odb_entry_t* ri_odb_entry_new(ri_odb_t *odb, uint64_t id, const ri_object_t *object)
{
    ri_odb_pool_t *pool = &odb->pool;

    if (pool->n >= pool->cap) {
        unsigned cap = 2 * pool->cap;
        void *tmp;

        tmp = realloc(pool->entries, cap * sizeof(pool->entries[0]));

        if (!tmp)
            return NULL;

        pool->entries = tmp;
        pool->cap = cap;
    }

    ri_odb_entry_t *entry = &pool->entries[pool->n++];

    *entry = (ri_odb_entry_t) {
        .id = id,
        .object = *object,
    };

    return entry;
}



int ri_odb_add_consumer_object(ri_odb_t *odb, uint64_t id, const ri_object_t *object, ri_odb_map_consumer_fn map_cb, ri_odb_unmap_fn unmap_cb, void *user_data)
{
    if (!ri_object_valid(object))
        return -EINVAL;

    ri_odb_entry_t *entry = ri_odb_entry_new(odb, id, object);

    if (!entry)
        return -ENOMEM;

    entry->cns_map_cb = map_cb;
    entry->unmap_cb = unmap_cb;
    entry->user_data = user_data;

    ri_odb_list_append(&odb->consumers.unassigned, entry);

    return 0;
}



int ri_odb_add_producer_object(ri_odb_t *odb, uint64_t id, const ri_object_t *object, ri_odb_map_producer_fn map_cb, ri_odb_unmap_fn unmap_cb, void *user_data)
{
    if (!ri_object_valid(object))
        return -EINVAL;

    ri_odb_entry_t *entry = ri_odb_entry_new(odb, id, object);

    if (!entry)
        return -ENOMEM;

    entry->prd_map_cb = map_cb;
    entry->unmap_cb = unmap_cb;
    entry->user_data = user_data;

    ri_odb_list_append(&odb->producers.unassigned, entry);

    return 0;
}


int ri_odb_consumer_channel_add_object(ri_odb_t *odb, unsigned chn_id, uint64_t obj_id, const ri_object_t *object, ri_odb_map_consumer_fn map_cb, ri_odb_unmap_fn unmap_cb, void *user_data)
{
    if (!ri_object_valid(object))
        return -EINVAL;

    if ((chn_id >= odb->consumers.n))
        return -EINVAL;

    ri_odb_entry_t *entry = ri_odb_entry_new(odb, obj_id, object);

    if (!entry)
        return -ENOMEM;

    entry->cns_map_cb = map_cb;
    entry->unmap_cb = unmap_cb;
    entry->user_data = user_data;

    ri_odb_list_append(&odb->consumers.channels[chn_id], entry);

    return 0;
}



int ri_odb_producer_channel_add_object(ri_odb_t *odb, unsigned chn_id, uint64_t obj_id, const ri_object_t *object, ri_odb_map_producer_fn map_cb, ri_odb_unmap_fn unmap_cb, void *user_data)
{
    if (!ri_object_valid(object))
        return -EINVAL;

    if ((chn_id >= odb->producers.n))
        return -EINVAL;

    ri_odb_entry_t *entry = ri_odb_entry_new(odb, obj_id, object);

    if (!entry)
        return -ENOMEM;

    entry->prd_map_cb = map_cb;
    entry->unmap_cb = unmap_cb;
    entry->user_data = user_data;

    ri_odb_list_append(&odb->producers.channels[chn_id], entry);

    return 0;
}



static int ri_odb_assign_object(ri_odb_group_t *group, uint64_t obj_id, unsigned chn_id)
{
    if (chn_id >= group->n)
        return -EINVAL;

    ri_odb_entry_t *entry = ri_odb_list_get(&group->unassigned, obj_id);

    if (!entry)
        return -ENOENT;

    ri_odb_list_move(entry, &group->channels[chn_id], &group->unassigned);

    return 0;
}


int ri_odb_assign_producer_object(ri_odb_t *odb, uint64_t obj_id, unsigned chn_id)
{
    return ri_odb_assign_object(&odb->producers, obj_id, chn_id);
}


int ri_odb_assign_consumer_object(ri_odb_t *odb, uint64_t obj_id, unsigned chn_id)
{
    return ri_odb_assign_object(&odb->consumers, obj_id, chn_id);
}


static int ri_odb_pool_alloc(ri_odb_pool_t *pool)
{
    pool->cap = RI_ODB_POOL_INIT_CAP;
    pool->n = 0;
    pool->entries = malloc(pool->cap * sizeof(ri_odb_entry_t));

    if (!pool->entries)
        return -ENOMEM;

    return 0;
}


static int ri_odb_group_alloc(ri_odb_group_t *group, unsigned n)
{
    group->n = n;

    if (group->n == 0)
        return 0;

    group->channels =  malloc(group->n * sizeof(ri_odb_list_t));

    if (!group->channels)
        return -ENOMEM;

    group->unassigned.first = NULL;
    group->unassigned.last = &group->unassigned.first;

    for (unsigned i = 0; i < n; i++) {
        group->channels[i].first = NULL;
        group->channels[i].last = &group->channels[i].first;
    }

    return 0;
}


void ri_odb_delete(ri_odb_t *odb)
{
    if (odb->producers.channels)
        free(odb->producers.channels);

    if (odb->consumers.channels)
        free(odb->consumers.channels);

    if (odb->pool.entries)
        free(odb->pool.entries);

    free(odb);
}



ri_odb_t *ri_odb_new(unsigned max_consumer_channels, unsigned max_producer_channels)
{
    ri_odb_t *odb = calloc(1, sizeof(ri_odb_t));

    if (!odb)
        goto fail_alloc;

    int r = ri_odb_pool_alloc(&odb->pool);

    if (r < 0)
        goto fail_pool_alloc;

    r = ri_odb_group_alloc(&odb->consumers, max_consumer_channels);

    if (r < 0)
        goto fail_pool_consumers;

    r = ri_odb_group_alloc(&odb->producers, max_producer_channels);

    if (r < 0)
        goto fail_pool_producers;

    return odb;

fail_pool_producers:
fail_pool_consumers:
fail_pool_alloc:
    ri_odb_delete(odb);
fail_alloc:
    return NULL;
}


