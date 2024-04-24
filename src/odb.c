#include "rtipc/odb.h"

#include <errno.h>
#include <stdlib.h>

#include "rtipc/mapper.h"
#include "mem_utils.h"
#include "log.h"

#define DEFAUT_CAP 16

typedef struct objcet_vector {
    unsigned num;
    unsigned cap;
    ri_object_meta_t *objects;
} object_vector_t;


struct ri_consumer_vector {
    object_vector_t vec;
    ri_odb_t *odb;
};


struct ri_producer_vector {
    object_vector_t vec;
    ri_odb_t *odb;
};


struct ri_odb {
    unsigned num_consumers;
    unsigned num_producers;
    ri_consumer_vector_t *consumers;
    ri_producer_vector_t *producers;
};


static unsigned count_objects(ri_odb_t *collection)
{
    unsigned count = 0;

    for (unsigned i = 0; i < collection->num_consumers; i++) {
        count += collection->consumers[i].vec.num;
    }

    for (unsigned i = 0; i < collection->num_producers; i++) {
        count += collection->producers[i].vec.num;
    }

    return count;
}


static int objcet_vector_add(object_vector_t *vec, const ri_object_meta_t *meta)
{
    if (!ri_object_valid(meta))
        return -EINVAL;

    if (vec->num + 1 >= vec->cap) {
        unsigned new_cap = vec->cap * 2;
        void *new_arr = realloc(vec->objects, new_cap * sizeof(ri_object_meta_t));

        if (!new_arr)
            return -ENOMEM;

        vec->objects = new_arr;
        vec->cap = new_cap;
    }

    vec->objects[vec->num] = *meta;
    vec->num++;

    return vec->num;
}


static void objcet_vector_delete(object_vector_t *vec)
{
    if (vec->objects)
        free(vec->objects);

    vec->cap = 0;
    vec->num = 0;
}



static int objcet_vector_init(object_vector_t *vec, unsigned cap)
{
    if (vec->objects)
        free(vec->objects);


    *vec = (object_vector_t) { 0 };

    vec->objects = malloc(cap * sizeof(ri_object_meta_t));

    if (!vec->objects)
        return -ENOMEM;

    vec->cap = cap;

    return vec->cap;
}


static void object_vector_dump(const object_vector_t *vec)
{
    LOG_INF("\tvector: num=%u cap=%u", vec->num, vec->cap);
    for (unsigned i = 0; i < vec->num; i++) {
        const ri_object_meta_t *object = &vec->objects[i];
        LOG_INF("\t\tobject[%u]: id=0x%lx size=%u align=%u", i, object->id, object->size, object->align);
    }
}

ri_odb_t* ri_odb_new(unsigned num_consumers, unsigned num_producers)
{
    ri_odb_t *odb = malloc(sizeof(ri_odb_t));

    if (!odb)
        goto fail_alloc;

    *odb = (ri_odb_t) {
      .num_consumers = num_consumers,
      .num_producers = num_producers,
    };

    if (num_consumers > 0) {
        odb->consumers = calloc(num_consumers, sizeof(ri_consumer_vector_t));

        if (!odb->consumers)
            goto fail_alloc_consumers;

        for (unsigned i = 0; i < num_consumers; i++) {
            odb->consumers[i].odb = odb;

            int r = objcet_vector_init(&odb->consumers[i].vec, DEFAUT_CAP);

            if (r < 0)
                goto fail_init_consumers;
        }
    }

    if (num_producers > 0) {
        odb->producers = calloc(num_producers, sizeof(ri_producer_vector_t));

        if (!odb->producers)
            goto fail_alloc_producers;

        for (unsigned i = 0; i < num_producers; i++) {
            odb->producers[i].odb = odb;

            int r = objcet_vector_init(&odb->producers[i].vec, DEFAUT_CAP);

            if (r < 0)
                goto fail_init_producers;
        }
    }


    return odb;


fail_init_producers:
fail_alloc_producers:
fail_init_consumers:
fail_alloc_consumers:
    ri_odb_delete(odb);
fail_alloc:
    return NULL;
}

static size_t get_channel_buffer_size(const object_vector_t *vec)
{
    size_t buffer_size = 0;

    for (unsigned i = 0; i < vec->num; i++)
        buffer_size = ri_object_get_offset_next(buffer_size, &vec->objects[i]);

    return buffer_size;
}


static void copy_metas(ri_object_meta_t metas[], const object_vector_t *vec)
{

    for ( unsigned i = 0; i < vec->num; i++)
        metas[i] = vec->objects[i];
}


static ri_shm_mapper_t* odb_create_shm(ri_odb_t *odb, const char *name, mode_t mode)
{
    unsigned num_objects = count_objects(odb);

    if (num_objects == 0)
        return NULL;

    ri_object_meta_t* metas = calloc(num_objects, sizeof(ri_object_meta_t));

    if (!metas)
        return NULL;

    ri_channel_req_t *consumers = NULL;
    ri_channel_req_t *producers = NULL;

    unsigned meta_idx = 0;

    if (odb->num_consumers > 0) {
        consumers = calloc(odb->num_consumers + 1, sizeof(ri_channel_req_t));

        if (!consumers)
            goto fail;

        for (unsigned i = 0; i < odb->num_consumers; i++) {
            const object_vector_t *vec = &odb->consumers[i].vec;
            ri_channel_req_t *channel = &consumers[i];

            copy_metas(&metas[meta_idx], vec);

            channel->buffer_size = get_channel_buffer_size(vec);
            channel->meta.size = vec->num * sizeof(ri_object_meta_t);
            channel->meta.ptr = &metas[meta_idx];
            meta_idx += vec->num;
        }
    }

    if (odb->num_producers > 0) {
        producers = calloc(odb->num_producers + 1, sizeof(ri_channel_req_t));

        if (!producers)
            goto fail;

        for (unsigned i = 0; i < odb->num_producers; i++) {
            const object_vector_t *vec = &odb->producers[i].vec;
            ri_channel_req_t *channel = &producers[i];

            copy_metas(&metas[meta_idx], vec);

            channel->buffer_size = get_channel_buffer_size(vec);
            channel->meta.size = vec->num * sizeof(ri_object_meta_t);
            channel->meta.ptr = &metas[meta_idx];
            meta_idx += vec->num;
        }
    }

    ri_shm_t *shm = name ? ri_named_shm_new(consumers, producers, name, mode) : ri_anon_shm_new(consumers, producers);

    if (!shm)
        goto fail;

    ri_shm_mapper_t *mapper = ri_shm_mapper_new(shm);

    if (!mapper)
        goto fail_mapper;

    if (consumers)
        free(consumers);
    if (producers)
        free(producers);

    free(metas);


    return mapper;

fail_mapper:
    ri_shm_delete(shm);
fail:
    if (consumers)
        free(consumers);
    if (producers)
        free(producers);
    free(metas);

    return NULL;
}


void ri_odb_delete(ri_odb_t *odb)
{
    if (odb->consumers) {
        for (unsigned i = 0; i < odb->num_consumers; i++)
            objcet_vector_delete(&odb->consumers[i].vec);

        free(odb->consumers);
    }

    if (odb->producers) {
        for (unsigned i = 0; i < odb->num_producers; i++)
            objcet_vector_delete(&odb->producers[i].vec);

        free(odb->producers);
    }

    free(odb);
}


ri_shm_mapper_t* ri_odb_create_anon_shm(ri_odb_t *odb)
{
     return odb_create_shm(odb, NULL, 0);
}


ri_shm_mapper_t* ri_odb_create_named_shm(ri_odb_t *odb, const char *name, mode_t mode)
{
    if (!name)
        return NULL;

    return odb_create_shm(odb, name, mode);
}


void ri_odb_dump(const ri_odb_t *odb)
{
    LOG_INF("odb %u consumers:", odb->num_consumers);
    for (unsigned i = 0; i < odb->num_consumers; i++)
        object_vector_dump(&odb->consumers[i].vec);

    LOG_INF("odb %u producers:", odb->num_producers);
        for (unsigned i = 0; i < odb->num_producers; i++)
            object_vector_dump(&odb->producers[i].vec);

}


ri_consumer_vector_t* ri_odb_get_consumer_vector(ri_odb_t *odb, unsigned channel_index)
{
    if (channel_index >= odb->num_consumers)
        return NULL;

    return &odb->consumers[channel_index];
}


ri_producer_vector_t* ri_odb_get_producer_vector(ri_odb_t *odb, unsigned channel_index)
{
    if (channel_index >= odb->num_producers)
        return NULL;

    return &odb->producers[channel_index];
}


int ri_consumer_vector_add(ri_consumer_vector_t *consumer, const ri_object_meta_t *meta)
{
    return objcet_vector_add(&consumer->vec, meta);
}


int ri_producer_vector_add(ri_producer_vector_t *producer, const ri_object_meta_t *meta)
{
    return objcet_vector_add(&producer->vec, meta);
}


unsigned ri_consumer_vector_get_index(ri_consumer_vector_t *vec)
{
    return vec - vec->odb->consumers;
}


unsigned ri_producer_vector_get_index(ri_producer_vector_t *vec)
{
    return vec - vec->odb->producers;
}


int ri_odb_add_consumer_object(ri_odb_t *odb, unsigned channel_index, const ri_object_meta_t *meta)
{
    ri_consumer_vector_t *vec = ri_odb_get_consumer_vector(odb, channel_index);

    if (!vec)
        return -ENOENT;

    return ri_consumer_vector_add(vec, meta);
}


int ri_odb_add_producer_object(ri_odb_t *odb, unsigned channel_index, const ri_object_meta_t *meta)
{
    ri_producer_vector_t *vec = ri_odb_get_producer_vector(odb, channel_index);

    if (!vec)
        return -ENOENT;

    return ri_producer_vector_add(vec, meta);
}
