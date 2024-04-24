#pragma once

#include <rtipc/object.h>
#include <rtipc/rtipc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ri_shm_mapper ri_shm_mapper_t;

/**
 * @typedef ri_producer_object_t
 *
 * @brief data object, pointer will be mapped by consumer/producer on update
 */
typedef struct ri_producer_object ri_producer_object_t;

typedef struct ri_consumer_object ri_consumer_object_t;


/**
 * @typedef ri_consumer_mapper_t
 *
 * @brief consumer object mapper
 */
typedef struct ri_consumer_mapper ri_consumer_mapper_t;

/**
 * @typedef ri_producer_mapper_t
 *
 * @brief producer object mapper
 */
typedef struct ri_producer_mapper ri_producer_mapper_t;


ri_shm_mapper_t* ri_shm_mapper_new(ri_shm_t *shm);

void ri_shm_mapper_delete(ri_shm_mapper_t *shm_mapper);

void ri_shm_mapper_dump(const ri_shm_mapper_t *shm_mapper);


ri_shm_t* ri_shm_mapper_get_shm(const ri_shm_mapper_t* mapper);

ri_consumer_mapper_t* ri_shm_mapper_get_consumer(ri_shm_mapper_t *mapper, unsigned index);

ri_producer_mapper_t* ri_shm_mapper_get_producer(ri_shm_mapper_t *mapper, unsigned index);

void ri_producer_mapper_update(ri_producer_mapper_t *mapper);

int ri_consumer_mapper_update(ri_consumer_mapper_t *mapper);

int ri_producer_mapper_enable_cache(ri_producer_mapper_t *mapper);

void ri_producer_mapper_disable_cache(ri_producer_mapper_t *mapper);

ri_consumer_t* ri_consumer_get_channel(ri_consumer_mapper_t *mapper);

ri_producer_t* ri_producer_get_channel(ri_producer_mapper_t *mapper);

ri_consumer_object_t* ri_consumer_mapper_get_object(ri_consumer_mapper_t *mapper, unsigned index);

ri_producer_object_t* ri_producer_mapper_get_object(ri_producer_mapper_t *mapper, unsigned index);

ri_consumer_object_t* ri_consumer_mapper_find_object(ri_consumer_mapper_t *mapper, const ri_object_meta_t *meta);

ri_producer_object_t* ri_producer_mapper_find_object(ri_producer_mapper_t *mapper, const ri_object_meta_t *meta);

unsigned ri_consumer_mapper_get_index(const ri_consumer_mapper_t *mapper);

unsigned ri_producer_mapper_get_index(const ri_producer_mapper_t *mapper);

void ri_producer_mapper_dump(const ri_producer_mapper_t *mapper);

void ri_consumer_mapper_dump(const ri_consumer_mapper_t *mapper);

const ri_object_meta_t* ri_producer_object_get_meta(const ri_producer_object_t *object);

const ri_object_meta_t* ri_consumer_object_get_meta(const ri_consumer_object_t *object);

int ri_consumer_object_get(const ri_consumer_object_t *object, void *value);

int ri_producer_object_set(const ri_producer_object_t *object, const void *value);

void* ri_producer_object_get_pointer(const ri_producer_object_t *object);

const void* ri_consumer_object_get_pointer(const ri_consumer_object_t *object);

ri_consumer_mapper_t* ri_consumer_object_get_mapper(ri_consumer_object_t *object);

ri_producer_mapper_t* ri_producer_object_get_mapper(ri_producer_object_t *object);


#ifdef __cplusplus
}
#endif
