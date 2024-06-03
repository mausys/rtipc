#include "rtipc/mapper.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>


#include "log.h"
#include "mem_utils.h"


struct ri_consumer_object {
    ri_object_meta_t meta;
    size_t offset;
    ri_consumer_mapper_t *mapper;
    struct {
        ri_consumer_object_t *next;
        ri_consumer_object_t **prev;
        ri_conusmer_object_fn func;
        void *user_data;
    } callback;
};


struct ri_producer_object {
    ri_object_meta_t meta;
    size_t offset;
    ri_producer_mapper_t *mapper;
    struct {
        ri_producer_object_t *next;
        ri_producer_object_t **prev;
        ri_producer_object_fn func;
        void *user_data;
    } callback;
};


struct ri_producer_mapper {
    ri_shm_mapper_t *shm_mapper;
    ri_producer_t *producer;
    ri_producer_object_t *objects;
    unsigned num;
    void *buffer;
    void *cache;
    size_t buffer_size;
    struct {
        bool lock;
        ri_producer_object_t *head;
    } callback;
};


struct ri_consumer_mapper {
    ri_shm_mapper_t *shm_mapper;
    ri_consumer_t *consumer;
    ri_consumer_object_t *objects;
    unsigned num;
    void *buffer;
    size_t buffer_size;
    struct {
        bool lock;
        ri_consumer_object_t *head;
    } callback;
};


struct ri_shm_mapper {
    ri_shm_t *shm;
    unsigned num_consumers;
    unsigned num_producers;
    ri_producer_mapper_t *producers;
    ri_consumer_mapper_t *consumers;
};


static void delete_consumer_mapper(ri_consumer_mapper_t *consumer)
{
    if (consumer->objects) {
        free(consumer->objects);
        consumer->objects = NULL;
        consumer->num = 0;
    }
}


static void delete_producer_mapper(ri_producer_mapper_t *producer)
{
    if (producer->objects) {
        free(producer->objects);
        producer->objects = NULL;
        producer->num = 0;
    }
}


static void delete_consumer_mappers(ri_shm_mapper_t *mapper)
{
    if (!mapper->consumers)
        return;

    for (unsigned i = 0; i < mapper->num_consumers; i++)
        delete_consumer_mapper(&mapper->consumers[i]);

    free(mapper->consumers);

    mapper->consumers = NULL;
    mapper->num_consumers = 0;
}


static void delete_producer_mappers(ri_shm_mapper_t *mapper)
{
    if (!mapper->producers)
        return;

    for (unsigned i = 0; i < mapper->num_producers; i++)
        delete_producer_mapper(&mapper->producers[i]);

    free(mapper->producers);

    mapper->producers = NULL;
    mapper->num_producers = 0;
}


static int init_consumer_mapper(ri_consumer_mapper_t *mapper, ri_consumer_t *consumer)
{
    mapper->consumer = consumer;

    ri_span_t meta_span = ri_consumer_get_meta(mapper->consumer);

    if ((meta_span.size == 0) || (!meta_span.ptr))
        return -ENOENT;

    const ri_object_meta_t *metas = meta_span.ptr;

    unsigned num = meta_span.size / sizeof(ri_object_meta_t);

    if (num == 0) {
        LOG_ERR("empty consumer channel");
        return -ENOENT;
    }

    mapper->objects = calloc(num, sizeof(ri_consumer_object_t));

    if (!mapper->objects)
        return -ENOMEM;

    mapper->num = num;
    mapper->buffer_size = ri_consumer_get_buffer_size(mapper->consumer);

    size_t offset = 0;

    for (unsigned i = 0; i < num; i++) {
        mapper->objects[i] = (ri_consumer_object_t) {
            .mapper = mapper,
            .meta = metas[i],
            .offset = offset,
        };

        offset = ri_object_get_offset_next(offset, &metas[i]);

        if (offset > mapper->buffer_size) {
            LOG_ERR("object exeeds buffer size");
            goto fail;
        }
    }

    return num;

fail:
    free(mapper->objects);
    mapper->objects = NULL;
    mapper->num = 0;
    return -1;
}


static int init_producer_mapper(ri_producer_mapper_t *mapper, ri_producer_t *producer)
{
    mapper->producer = producer;

    ri_span_t meta_span = ri_producer_get_meta(mapper->producer);

    if ((meta_span.size == 0) || (!meta_span.ptr))
        return -ENOENT;

    const ri_object_meta_t *metas = meta_span.ptr;

    unsigned num = meta_span.size / sizeof(ri_object_meta_t);

    if (num == 0) {
        LOG_ERR("empty producer channel");
        return -ENOENT;
    }

    mapper->objects = calloc(num, sizeof(ri_producer_object_t));

    if (!mapper->objects)
        return -ENOMEM;

    mapper->num = num;
    mapper->buffer_size = ri_producer_get_buffer_size(mapper->producer);

    size_t offset = 0;

    for (unsigned i = 0; i < num; i++) {
        mapper->objects[i] = (ri_producer_object_t) {
            .mapper = mapper,
            .meta = metas[i],
            .offset = offset,
        };

        offset = ri_object_get_offset_next(offset, &metas[i]);

        if (offset > mapper->buffer_size) {
            LOG_ERR("object exeeds buffer size");
            goto fail;
        }
    }

    mapper->buffer = ri_producer_swap(mapper->producer);
    return num;

fail:
    free(mapper->objects);
    mapper->objects = NULL;
    mapper->num = 0;
    return -1;
}

static void consumer_object_add_callback(ri_consumer_object_t *object, ri_conusmer_object_fn callback, void *user_data)
{
    ri_consumer_mapper_t *mapper = object->mapper;

    object->callback.func = callback;
    object->callback.user_data = user_data;

    object->callback.next = mapper->callback.head;
    object->callback.prev = &mapper->callback.head;

    if (mapper->callback.head)
        mapper->callback.head->callback.prev = &object->callback.next;

    mapper->callback.head = object;
}


static void consumer_object_remove_callback(ri_consumer_object_t *object)
{
    object->callback.func = NULL;

    if (object->callback.next)
        object->callback.next->callback.prev = object->callback.prev;

    *object->callback.prev = object->callback.next;
}


static void producer_object_add_callback(ri_producer_object_t *object, ri_producer_object_fn callback, void *user_data)
{
    ri_producer_mapper_t *mapper = object->mapper;

    object->callback.func = callback;
    object->callback.user_data = user_data;

    object->callback.next = mapper->callback.head;
    object->callback.prev = &mapper->callback.head;

    if (mapper->callback.head)
        mapper->callback.head->callback.prev = &object->callback.next;

    mapper->callback.head = object;
}


static void producer_object_remove_callback(ri_producer_object_t *object)
{
    object->callback.func = NULL;

    if (object->callback.next)
        object->callback.next->callback.prev = object->callback.prev;

    *object->callback.prev = object->callback.next;
}


ri_shm_mapper_t* ri_shm_mapper_new(ri_shm_t *shm)
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
            ri_consumer_t *consumer = ri_shm_get_consumer(shm, i);
            ri_consumer_mapper_t *consumer_mapper = &shm_mapper->consumers[i];

            if (!consumer)
                goto fail_init_consumers;

            int r = init_consumer_mapper(consumer_mapper, consumer);

            if (r < 0)
                goto fail_init_consumers;

            consumer_mapper->shm_mapper = shm_mapper;
        }
    }

    if (shm_mapper->num_producers > 0) {
        shm_mapper->producers = calloc(shm_mapper->num_producers, sizeof(ri_producer_mapper_t));

        if (!shm_mapper->producers)
            goto fail_alloc_producers;

        for (unsigned i = 0; i < shm_mapper->num_producers; i++) {
            ri_producer_t *producer = ri_shm_get_producer(shm, i);
            ri_producer_mapper_t *producer_mapper = &shm_mapper->producers[i];

            if (!producer_mapper)
                goto fail_init_consumers;

            int r = init_producer_mapper(producer_mapper, producer);

            if (r < 0)
                goto fail_init_producers;

            producer_mapper->shm_mapper = shm_mapper;
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


ri_shm_mapper_t* ri_shm_mapper_map(int fd)
{
    ri_shm_t *shm = ri_shm_map(fd);

    if (!shm)
        return NULL;

    ri_shm_mapper_t* mapper = ri_shm_mapper_new(shm);

    if (!mapper) {
        ri_shm_delete(shm);
        return NULL;
    }

    return mapper;
}


void ri_shm_mapper_delete(ri_shm_mapper_t *shm_mapper)
{
    delete_producer_mappers(shm_mapper);
    delete_consumer_mappers(shm_mapper);
    free(shm_mapper);
}


bool ri_producer_mapper_ackd(const ri_producer_mapper_t *mapper)
{
    return ri_producer_consumed(mapper->producer);
}


ri_consumer_t* ri_consumer_mapper_get_channel(ri_consumer_mapper_t *mapper)
{
    return mapper->consumer;
}


ri_producer_t* ri_producer_mapper_get_channel(ri_producer_mapper_t *mapper)
{
    return mapper->producer;
}


void ri_consumer_mapper_dump(const ri_consumer_mapper_t *mapper)
{
    unsigned index = mapper - mapper->shm_mapper->consumers;

    LOG_INF("\tconsumer[%u] mapper buffer_size=%zu channel=%p", index, mapper->buffer_size, mapper->consumer);

    for (unsigned i = 0; i < mapper->num; i++) {
        const ri_consumer_object_t *object = &mapper->objects[i];
        LOG_INF("\t\tobject[%u] id=0x%lx size=%u align=%u offset=%zu", i,
                object->meta.id, object->meta.size, object->meta.align, object->offset);
    }
}


void ri_producer_mapper_dump(const ri_producer_mapper_t *mapper)
{
    unsigned index = mapper - mapper->shm_mapper->producers;

    LOG_INF("\tproducer[%u] mapper buffer_size=%zu channel=%p", index, mapper->buffer_size, mapper->producer);

    for (unsigned i = 0; i < mapper->num; i++) {
        const ri_producer_object_t *object = &mapper->objects[i];
        LOG_INF("\t\tobject[%u] id=0x%lx size=%u align=%u offset=%zu", i,
                object->meta.id, object->meta.size, object->meta.align, object->offset);

    }
}


void ri_shm_mapper_dump(const ri_shm_mapper_t *mapper)
{
    LOG_INF("mapper:");

    for (unsigned i = 0; i < mapper->num_consumers; i++)
        ri_consumer_mapper_dump(&mapper->consumers[i]);

    for (unsigned i = 0; i < mapper->num_producers; i++)
        ri_producer_mapper_dump(&mapper->producers[i]);
}


ri_shm_t* ri_shm_mapper_get_shm(const ri_shm_mapper_t *shm_mapper)
{
    return shm_mapper->shm;
}


int ri_shm_mapper_get_fd(const ri_shm_mapper_t *mapper)
{
    ri_shm_t* shm = ri_shm_mapper_get_shm(mapper);

    if (!shm)
        return -ENXIO;

    return ri_shm_get_fd(shm);
}


int ri_producer_mapper_enable_cache(ri_producer_mapper_t *mapper)
{
    if (mapper->cache)
        return 0;

    mapper->cache = calloc(1, mapper->buffer_size);

    if (!mapper->cache)
        return -ENOMEM;

    if (mapper->buffer)
        memcpy(mapper->cache, mapper->buffer, mapper->buffer_size);

    return 0;
}


void ri_producer_mapper_disable_cache(ri_producer_mapper_t *mapper)
{
    if (mapper->cache) {
        if (mapper->buffer)
            memcpy(mapper->cache, mapper->buffer, mapper->buffer_size);

        free(mapper->cache);
        mapper->cache = NULL;
    }
}



ri_consumer_mapper_t* ri_shm_mapper_get_consumer(ri_shm_mapper_t *shm_mapper, unsigned index)
{
    if (index >= shm_mapper->num_consumers)
        return NULL;

    return &shm_mapper->consumers[index];
}


ri_producer_mapper_t* ri_shm_mapper_get_producer(ri_shm_mapper_t *shm_mapper, unsigned index)
{
    if (index >= shm_mapper->num_producers)
        return NULL;

    return &shm_mapper->producers[index];
}


int ri_consumer_mapper_update(ri_consumer_mapper_t *mapper)
{
    if (mapper->callback.lock)
        return -EINVAL;

    void *old = mapper->buffer;

    mapper->buffer = ri_consumer_fetch(mapper->consumer);

    if (!mapper->buffer)
        return -1;

    if (old == mapper->buffer)
        return 0;

    mapper->callback.lock = true;

    for (ri_consumer_object_t *object = mapper->callback.head; object; object = object->callback.next) {
        if (object->callback.func) {
            const void *pointer = ri_consumer_object_get_pointer(object);
            object->callback.func(object, pointer, object->callback.user_data);
        } else {
            // should never happen
            LOG_ERR("ri_consumer_mapper_update null callback");
        }
    }

    mapper->callback.lock = false;

    return 1;
}


void ri_producer_mapper_update(ri_producer_mapper_t *mapper)
{
    if (mapper->callback.lock)
        return;

    mapper->callback.lock = true;

    for (ri_producer_object_t *object = mapper->callback.head; object; object = object->callback.next) {
        if (object->callback.func) {
            void *pointer = ri_producer_object_get_pointer(object);
            object->callback.func(object, pointer, object->callback.user_data);
        } else {
            // should never happen
            LOG_ERR("ri_producer_mapper_update null callback");
        }
    }

    mapper->callback.lock = false;

    if (mapper->cache)
        memcpy(mapper->buffer, mapper->cache, mapper->buffer_size);

    mapper->buffer = ri_producer_swap(mapper->producer);
}


ri_consumer_object_t* ri_consumer_mapper_get_object(ri_consumer_mapper_t *mapper, unsigned index)
{
    if (index >= mapper->num)
        return NULL;

    return &mapper->objects[index];
}


ri_producer_object_t* ri_producer_mapper_get_object(ri_producer_mapper_t *mapper, unsigned index)
{
    if (index >= mapper->num)
        return NULL;

    return &mapper->objects[index];
}


ri_consumer_object_t* ri_consumer_mapper_find_object(ri_consumer_mapper_t *mapper, const ri_object_meta_t *meta)
{
    for (unsigned i = 0; i < mapper->num; i++) {
        ri_consumer_object_t *object = &mapper->objects[i];

        if (ri_objects_equal(meta, &object->meta))
            return &mapper->objects[i];
    }

    return NULL;
}


ri_producer_object_t* ri_producer_mapper_find_object(ri_producer_mapper_t *mapper, const ri_object_meta_t *meta)
{
    for (unsigned i = 0; i < mapper->num; i++) {
        ri_producer_object_t *object = &mapper->objects[i];

        if (ri_objects_equal(meta, &object->meta))
            return &mapper->objects[i];
    }

    return NULL;
}


unsigned ri_consumer_mapper_get_index(const ri_consumer_mapper_t *mapper)
{
    return mapper - mapper->shm_mapper->consumers;
}


unsigned ri_producer_mapper_get_index(const ri_producer_mapper_t *mapper)
{
    return mapper - mapper->shm_mapper->producers;
}


int ri_producer_object_copy(const ri_producer_object_t *object, const void *content)
{
    void *ptr = object->mapper->cache;

    if (!ptr)
        ptr = object->mapper->buffer;

    if (!ptr)
        return -EAGAIN;

    ptr = mem_offset(ptr, object->offset);
    memcpy(ptr, content, object->meta.size);

    return object->meta.size;
}


int ri_consumer_object_copy(const ri_consumer_object_t *object, void *content)
{
    if (!object->mapper->buffer)
        return -EAGAIN;

    const void *ptr = mem_offset(object->mapper->buffer, object->offset);
    memcpy(content, ptr, object->meta.size);

    return object->meta.size;
}



void* ri_producer_object_get_pointer(const ri_producer_object_t *object)
{
    void *ptr = object->mapper->cache;

    if (!ptr)
        ptr = object->mapper->buffer;

    if (!ptr)
        return NULL;

    return mem_offset(ptr, object->offset);
}


const void* ri_consumer_object_get_pointer(const ri_consumer_object_t *object)
{
    void *ptr = object->mapper->buffer;

    if (!ptr)
        return NULL;

    return mem_offset(ptr, object->offset);
}


const ri_object_meta_t* ri_producer_object_get_meta(const ri_producer_object_t *object)
{
    return &object->meta;
}

const ri_object_meta_t* ri_consumer_object_get_meta(const ri_consumer_object_t *object)
{
    return &object->meta;
}


ri_consumer_mapper_t* ri_consumer_object_get_mapper(ri_consumer_object_t *object)
{
    return object->mapper;
}


ri_producer_mapper_t* ri_producer_object_get_mapper(ri_producer_object_t *object)
{
    return object->mapper;
}


void ri_consumer_object_set_callback(ri_consumer_object_t *object, ri_conusmer_object_fn callback, void *user_data)
{
    if (!object->callback.func && callback) {
        consumer_object_add_callback(object, callback, user_data);
    } else if (object->callback.func && !callback) {
        consumer_object_remove_callback(object);
    } else {
        object->callback.func = callback;
        object->callback.user_data = user_data;
    }
}


void ri_producer_object_set_callback(ri_producer_object_t *object, ri_producer_object_fn callback, void *user_data)
{
    if (!object->callback.func && callback) {
        producer_object_add_callback(object, callback, user_data);
    } else if (object->callback.func && !callback) {
        producer_object_remove_callback(object);
    } else {
        object->callback.func = callback;
        object->callback.user_data = user_data;
    }
}



