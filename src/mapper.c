#include "rtipc.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>


#include "log.h"
#include "mem_utils.h"

typedef struct {
    ri_object_meta_t meta;
    size_t offset;
    void **ptr;
} mapper_object_t;


struct ri_producer_mapper {
    ri_producer_t *channel;
    mapper_object_t *objects;
    unsigned num;
    void *cache;
    void *buffer;
    size_t buffer_size;
};


struct ri_consumer_mapper {
    ri_consumer_t *channel;
    mapper_object_t *objects;
    unsigned num;
    void *buffer;
    size_t buffer_size;
};


struct ri_shm_mapper {
    ri_shm_t *shm;
    unsigned num_consumers;
    unsigned num_producers;
    ri_producer_mapper_t *producers;
    ri_consumer_mapper_t *consumers;
};



static unsigned count_objects(const ri_object_t objects[])
{
    unsigned i;
    for (i = 0; objects[i].meta.size != 0; i++)
        ;
    return i;
}


static unsigned count_channels(ri_object_t *channels[])
{
    unsigned n = 0;
    if (channels) {
        for (ri_object_t **channel = channels; *channel; channel++)
            n++;
    }

    return n;
}


static void nullify_opjects(mapper_object_t objects[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (objects[i].ptr)
            *objects[i].ptr = NULL;
    }
}


static unsigned copy_meta(ri_object_meta_t meta_array[], const ri_object_t channels[])
{
    unsigned n = 0;
    if (channels) {
        for (const ri_object_t *object = channels; object->meta.size != 0; object++)
            memcpy(&meta_array[n++], &object->meta, sizeof(object->meta));
    }

    return n;
}


static void map_opjects(void *ptr, mapper_object_t objects[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (objects[i].ptr)
            *objects[i].ptr = mem_offset(ptr, objects[i].offset);
    }
}


static void objects_dump(const mapper_object_t *objects, unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        const mapper_object_t *o = &objects[i];
        LOG_INF("object id=%lu size=%u align=%u offset=%zu ptr=%p", o->meta.id, o->meta.size, o->meta.align, o->offset, o->ptr);
    }
}


static ri_object_meta_t* create_meta_array(ri_object_t *channels[])
{
    unsigned num = 0;

    if (channels) {
        for (ri_object_t **channel = channels; *channel; channel++)
            num += count_objects(*channel);
    }

    if (num == 0)
        return NULL;

    ri_object_meta_t *meta_array = calloc(num, sizeof(ri_object_meta_t));

     if (!meta_array)
        return NULL;

      unsigned meta_idx = 0;

    for (ri_object_t **channel = channels; *channel; channel++)
        meta_idx += copy_meta(&meta_array[meta_idx], *channel);

    return meta_array;
}



size_t ri_channel_objects_buffer_size(const ri_object_t objects[])
{
    size_t size = 0;

    for (unsigned i = 0; objects[i].meta.size != 0; i++) {
        if (objects[i].meta.align != 0)
            size = mem_align(size, objects[i].meta.align);

        size += objects[i].meta.size;
    }

    return size;
}


static size_t channel_meta_buffer_size(const ri_object_meta_t meta[], unsigned n)
{
    size_t size = 0;

    for (unsigned i = 0; i < n; i++) {
        if (meta[i].align != 0)
            size = mem_align(size, meta[i].align);

        size += meta[i].size;
    }

    return size;
}


static void delete_consumer_mapper(ri_consumer_mapper_t* mapper)
{
    if (mapper->objects) {
        nullify_opjects(mapper->objects, mapper->num);
        free(mapper->objects);
        mapper->objects = NULL;
    }
}


static void delete_producer_mapper(ri_producer_mapper_t* mapper)
{
    if (mapper->objects) {
        nullify_opjects(mapper->objects, mapper->num);
        free(mapper->objects);
        mapper->objects = NULL;
    }

    if (mapper->cache) {
        free(mapper->cache);
        mapper->cache = NULL;
    }
}


static void delete_consumer_mappers(ri_shm_mapper_t *shm_mapper)
{
    if (!shm_mapper->consumers)
        return;

    for (unsigned i = 0; i < shm_mapper->num_consumers; i++)
        delete_consumer_mapper(&shm_mapper->consumers[i]);

    free(shm_mapper->consumers);

    shm_mapper->consumers = NULL;
    shm_mapper->num_consumers = 0;
}


static void delete_producer_mappers(ri_shm_mapper_t *shm_mapper)
{
    if (!shm_mapper->producers)
        return;

    for (unsigned i = 0; i < shm_mapper->num_producers; i++)
        delete_producer_mapper(&shm_mapper->producers[i]);

    free(shm_mapper->producers);

    shm_mapper->producers = NULL;
    shm_mapper->num_producers = 0;
}


static mapper_object_t* mapper_objects_new(const ri_object_meta_t *meta, unsigned n, size_t *buffer_size)
{
        mapper_object_t *objects = calloc(n, sizeof(mapper_object_t));

        if (!objects)
        return NULL;

        size_t offset = 0;

        for (unsigned i = 0; i < n; i++) {
            if (!ri_object_valid(&meta[i])) {
                LOG_ERR("got invalid object [%u] from consumer channel", i);
                goto fail;
            }

            objects[i].meta = meta[i];
            objects[i].offset = offset;

            if (meta[i].align != 0)
                offset = mem_align(offset, meta[i].align);

            offset += meta[i].size;
            }

            if (offset > *buffer_size) {
            LOG_ERR("objects size exeeds channel size; objects size=%zu, channel size=%zu", offset, *buffer_size);
            goto fail;
        }

        *buffer_size = offset;

        return objects;

fail:
        free(objects);
        return NULL;
}


static bool mapper_object_check(mapper_object_t *mapper, const ri_object_t *object)
{
        return (mapper->meta.align == object->meta.align)
            && (mapper->meta.size == object->meta.size)
            && (!mapper->ptr);
}


static mapper_object_t* search_mapper_objects(mapper_object_t mappers[], unsigned num, const ri_object_t *object)
{
        for (unsigned i = 0; i < num; i++) {
            if (mappers[i].meta.id == object->meta.id) {
                if (!mapper_object_check(&mappers[i], object))
                    return NULL;
                return &mappers[i];
            }
        }
        return NULL;
}




static int object_mapper_assign(mapper_object_t mappers[], unsigned num, const ri_object_t objects[], bool use_id)
{
        if (use_id) {
            for (unsigned i = 0; objects[i].meta.size != 0; i++) {
                mapper_object_t *mapper = search_mapper_objects(mappers, num, &objects[i]);

                if (!mapper)
                    return -ENOENT;

                mapper->ptr = objects[i].ptr;
            }
        } else {
            for (unsigned i = 0; objects[i].meta.size != 0; i++) {
                if (i >= num)
                    return -ENOENT;

                if (!mapper_object_check(&mappers[i],  &objects[i]))
                    return -ENOENT;

                mappers[i].ptr =  objects[i].ptr;
            }
        }

        return 0;
}


static int init_consumer_mapper(ri_consumer_mapper_t *mapper, ri_consumer_t *channel)
{
    mapper->channel = channel;

    ri_span_t meta_span = ri_consumer_get_meta(mapper->channel);

    if (meta_span.size == 0)
        return -ENOENT;


    mapper->num = meta_span.size / sizeof(ri_object_meta_t);

    if (mapper->num == 0) {
        LOG_ERR("empty producer channel");
        return -ENOENT;
    }
;

    mapper->buffer_size = ri_consumer_get_buffer_size(mapper->channel);

    mapper->objects = mapper_objects_new(meta_span.ptr, mapper->num, &mapper->buffer_size);

    if (!mapper->objects)
        return -ENOMEM;

    return 0;
}


static int init_producer_mapper(ri_producer_mapper_t *mapper, ri_producer_t *channel)
{
    mapper->cache = NULL;
    mapper->channel = channel;

    ri_span_t meta_span = ri_producer_get_meta(mapper->channel);

    if (meta_span.size == 0)
        return -ENOENT;

    mapper->num = meta_span.size / sizeof(ri_object_meta_t);

    if (mapper->num == 0) {
        LOG_ERR("empty producer channel");
        return -ENOENT;
    }


    mapper->buffer_size = ri_producer_get_buffer_size(mapper->channel);

    mapper->objects = mapper_objects_new(meta_span.ptr, mapper->num, &mapper->buffer_size);

    if (!mapper->objects)
        return -ENOMEM;

    return 0;
}



static ri_shm_mapper_t* shm_mapper_new(ri_shm_t *shm)
{
    ri_shm_mapper_t *shm_mapper = calloc(1, sizeof(ri_shm_mapper_t));

    if (!shm_mapper)
        goto fail_alloc;

    shm_mapper->shm = shm;

    shm_mapper->num_consumers = ri_shm_get_num_consumers(shm);
    shm_mapper->num_producers = ri_shm_get_num_producers(shm);

    if (shm_mapper->num_consumers > 0) {
        shm_mapper->consumers = calloc(shm_mapper->num_consumers, sizeof(ri_consumer_mapper_t));

        if (!shm_mapper->consumers)
                goto fail_alloc_consumers;

        for (unsigned i = 0; i < shm_mapper->num_consumers; i++) {
            ri_consumer_t *channel = ri_shm_get_consumer(shm, i);
            ri_consumer_mapper_t *mapper = &shm_mapper->consumers[i];

            if (!channel)
                goto fail_init_consumers;

            int r = init_consumer_mapper(mapper, channel);

            if (r < 0)
                goto fail_init_consumers;
        }
    }

    if (shm_mapper->num_producers > 0) {
        shm_mapper->producers = calloc(shm_mapper->num_producers, sizeof(ri_producer_mapper_t));

        if (!shm_mapper->producers)
            goto fail_alloc_producers;

        for (unsigned i = 0; i < shm_mapper->num_producers; i++) {
            ri_producer_t *channel = ri_shm_get_producer(shm, i);
            ri_producer_mapper_t *mapper = &shm_mapper->producers[i];

            if (!channel)
                goto fail_init_consumers;

            int r = init_producer_mapper(mapper, channel);

            if (r < 0)
                goto fail_init_producers;
        }
    }

    return shm_mapper;

fail_init_producers:
    delete_producer_mappers(shm_mapper);
fail_alloc_producers:
fail_init_consumers:
    delete_consumer_mappers(shm_mapper);
fail_alloc_consumers:
    free(shm_mapper);
fail_alloc:
    return NULL;

}


static ri_shm_mapper_t* server_shm_create(ri_object_t *consumer_channels[], ri_object_t *producer_channels[], const char *name, mode_t mode)
{
    unsigned num_consumers = count_channels(consumer_channels);
    unsigned num_producers = count_channels(producer_channels);

    ri_object_meta_t* meta_consumers = create_meta_array(consumer_channels);

    if (!meta_consumers)
        goto fail_meta_consumers;

    ri_object_meta_t* meta_producers = create_meta_array(producer_channels);

    if (!meta_producers)
        goto fail_meta_producers;

    ri_channel_description_t *consumers = calloc(num_consumers + 1, sizeof(ri_channel_description_t));

    if (!consumers)
        goto fail_consumers;

    ri_channel_description_t *producers = calloc(num_producers + 1, sizeof(ri_channel_description_t));

    if (!producers)
        goto fail_producers;

    unsigned object_idx = 0;

    for (unsigned i = 0; i < num_consumers; i++) {
        ri_channel_description_t *channel = &consumers[i];
        const ri_object_t *channel_objects = consumer_channels[i];

        unsigned num_objects = count_objects(channel_objects);

        channel->buffer_size = ri_channel_objects_buffer_size(channel_objects);
        channel->meta.size = num_objects * sizeof(ri_object_meta_t);
        channel->meta.ptr = &meta_consumers[object_idx];

        object_idx += num_objects;
    }

    object_idx = 0;

    for (unsigned i = 0; i < num_producers; i++) {
        ri_channel_description_t *channel = &producers[i];
        const ri_object_t *channel_objects = producer_channels[i];

        unsigned num_objects = count_objects(channel_objects);

        channel->buffer_size = ri_channel_objects_buffer_size(channel_objects);
        channel->meta.size = num_objects * sizeof(ri_object_meta_t);
        channel->meta.ptr = &meta_producers[object_idx];

        object_idx += num_objects;
    }


    ri_shm_t *shm = name ? ri_named_shm_new(consumers, producers, name, mode) : ri_anon_shm_new(consumers, producers);

    if (!shm)
        goto fail_shm;

    ri_shm_mapper_t *shm_mapper = shm_mapper_new(shm);

    if (!shm_mapper)
        goto fail_mapper;

    for (unsigned i = 0; i < num_consumers; i++) {
        ri_consumer_mapper_t *mapper = &shm_mapper->consumers[i];
        int r = object_mapper_assign(mapper->objects, mapper->num, consumer_channels[i], false);

        if (r < 0)
            goto fail_mapper;
    }

    for (unsigned i = 0; i < num_producers; i++) {
        ri_producer_mapper_t *mapper = &shm_mapper->producers[i];
        int r = object_mapper_assign(mapper->objects, mapper->num, producer_channels[i], false);

        if (r < 0)
            goto fail_mapper;

        mapper->buffer = ri_producer_swap(mapper->channel);

        map_opjects(mapper->buffer, mapper->objects, mapper->num);
    }

    free(meta_consumers);
    free(meta_producers);
    free(consumers);
    free(producers);



    return shm_mapper;

fail_mapper:
    ri_shm_delete(shm);
fail_shm:
    free(producers);
fail_producers:
    free(consumers);
fail_consumers:
    free(meta_producers);
fail_meta_producers:
    free(meta_consumers);
fail_meta_consumers:
    return NULL;
}


bool ri_producer_mapper_ackd(const ri_producer_mapper_t *mapper)
{
    return ri_producer_ackd(mapper->channel);
}


ri_consumer_t* ri_consumer_get_channel(ri_consumer_mapper_t *mapper)
{
    return mapper->channel;
}


ri_producer_t* ri_producer_get_channel(ri_producer_mapper_t *mapper)
{
    return mapper->channel;
}


void ri_consumer_mapper_dump(const ri_consumer_mapper_t *mapper)
{
    LOG_INF("consumer_mapper objects(%u):", mapper->num);
    objects_dump(mapper->objects, mapper->num);
}


void ri_producer_mapper_dump(const ri_producer_mapper_t* mapper)
{
    LOG_INF("producer_mapper objects(%u):", mapper->num);
    objects_dump(mapper->objects, mapper->num);
}


ri_shm_t* ri_shm_mapper_get_shm(const ri_shm_mapper_t* shm_mapper)
{
    return shm_mapper->shm;
}

ri_consumer_mapper_t* ri_shm_mapper_get_consumer(ri_shm_mapper_t* shm_mapper, unsigned index)
{
    if (index >= shm_mapper->num_consumers)
        return NULL;

    return &shm_mapper->consumers[index];
}

ri_producer_mapper_t* ri_shm_mapper_get_producer(ri_shm_mapper_t* shm_mapper, unsigned index)
{
    if (index >= shm_mapper->num_producers)
        return NULL;

    return &shm_mapper->producers[index];
}

ri_shm_mapper_t* ri_server_anon_shm_mapper_new(ri_object_t *consumer_channels[], ri_object_t *producer_channels[])
{
    return server_shm_create(consumer_channels, producer_channels, NULL, 0);
}


ri_shm_mapper_t* ri_server_named_shm_mapper_new(ri_object_t *consumer_channels[], ri_object_t *producer_channels[], const char *name, mode_t mode)
{
    if (!name)
        return NULL;

    return server_shm_create(consumer_channels, producer_channels, name, mode);
}


ri_shm_mapper_t* ri_client_shm_mapper_new(int fd, ri_object_t *consumer_channels[], ri_object_t *producer_channels[])
{
    ri_shm_t *shm = ri_shm_map(fd);

    if (!shm)
        return NULL;

    ri_shm_mapper_t *shm_mapper = shm_mapper_new(shm);

    if (!shm_mapper) {
        ri_shm_delete(shm);
        return NULL;
    }

    unsigned num_consumers = count_channels(consumer_channels);
    unsigned num_producers = count_channels(producer_channels);

    for (unsigned i = 0; i < num_consumers; i++) {
        ri_consumer_mapper_t *mapper = &shm_mapper->consumers[i];
        int r = object_mapper_assign(mapper->objects, mapper->num, consumer_channels[i], false);

        if (r < 0)
            goto fail_mapper;
    }

    for (unsigned i = 0; i < num_producers; i++) {
        ri_producer_mapper_t *mapper = &shm_mapper->producers[i];
        int r = object_mapper_assign(mapper->objects, mapper->num, producer_channels[i], false);

        if (r < 0)
            goto fail_mapper;

        mapper->buffer = ri_producer_swap(mapper->channel);

        map_opjects(mapper->buffer, mapper->objects, mapper->num);
    }

    return shm_mapper;


fail_mapper:
    ri_shm_mapper_delete(shm_mapper);
    return NULL;
}


ri_shm_mapper_t* ri_client_named_shm_mapper_new(const char *name, ri_object_t *consumer_channels[], ri_object_t *producer_channels[])
{
    if (!name)
        return NULL;

    ri_shm_t *shm = ri_named_shm_map(name);

    if (!shm)
        return NULL;

    ri_shm_mapper_t *shm_mapper = shm_mapper_new(shm);

    if (!shm_mapper) {
        ri_shm_delete(shm);
        return NULL;
    }

    unsigned num_consumers = count_channels(consumer_channels);
    unsigned num_producers = count_channels(producer_channels);

    for (unsigned i = 0; i < num_consumers; i++) {
        ri_consumer_mapper_t *mapper = &shm_mapper->consumers[i];

        if (!mapper)
            goto fail_mapper;

        int r = object_mapper_assign(mapper->objects, mapper->num, consumer_channels[i], false);

        if (r < 0)
            goto fail_mapper;
    }

    for (unsigned i = 0; i < num_producers; i++) {
        ri_producer_mapper_t *mapper = &shm_mapper->producers[i];

        if (!mapper)
             goto fail_mapper;

        int r = object_mapper_assign(mapper->objects, mapper->num, producer_channels[i], false);

        if (r < 0)
            goto fail_mapper;

        mapper->buffer = ri_producer_swap(mapper->channel);

        map_opjects(mapper->buffer, mapper->objects, mapper->num);
    }

    return shm_mapper;


fail_mapper:
    ri_shm_mapper_delete(shm_mapper);
    return NULL;
}


void ri_shm_mapper_delete(ri_shm_mapper_t* shm_mapper)
{
    delete_consumer_mappers(shm_mapper);

    delete_producer_mappers(shm_mapper);

    ri_shm_delete(shm_mapper->shm);

    free(shm_mapper);
}



int ri_consumer_mapper_update(ri_consumer_mapper_t *mapper)
{
    void *old = mapper->buffer;

    mapper->buffer = ri_consumer_fetch(mapper->channel);

    if (!mapper->buffer) {
        nullify_opjects(mapper->objects, mapper->num);
        return -1;
    }

    if (old == mapper->buffer)
        return 0;

    map_opjects(mapper->buffer, mapper->objects, mapper->num);

    return 1;
}


void ri_producer_mapper_update(ri_producer_mapper_t *mapper)
{
    if (mapper->cache) {
        memcpy(mapper->buffer, mapper->cache, mapper->buffer_size);
        mapper->buffer = ri_producer_swap(mapper->channel);
    } else {
        mapper->buffer = ri_producer_swap(mapper->channel);
        map_opjects(mapper->buffer, mapper->objects, mapper->num);
    }
}


int ri_producer_mapper_enable_cache(ri_producer_mapper_t* mapper)
{
    if (mapper->cache)
        return 0;

    mapper->cache = malloc(mapper->buffer_size);

    if (!mapper->cache)
        return -ENOMEM;

    if (mapper->buffer)
        memcpy(mapper->cache, mapper->buffer, mapper->buffer_size);

    map_opjects(mapper->cache, mapper->objects, mapper->num);

    return 0;
}


void ri_producer_mapper_disable_cache(ri_producer_mapper_t* mapper)
{
    if (!mapper->cache)
        return;

    if (mapper->buffer) {
        memcpy (mapper->buffer, mapper->cache, mapper->buffer_size);
        map_opjects(mapper->buffer, mapper->objects, mapper->num);
    } else {
        nullify_opjects(mapper->objects, mapper->num);
    }

    free(mapper->cache);
    mapper->cache = NULL;
}


