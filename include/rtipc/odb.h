#pragma once

#include <rtipc/mapper.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct ri_odb ri_odb_t;
typedef struct ri_consumer_vector ri_consumer_vector_t;
typedef struct ri_producer_vector ri_producer_vector_t;


ri_odb_t* ri_odb_new(unsigned num_consumers, unsigned num_producers);
void ri_odb_delete(ri_odb_t *odb);

ri_shm_mapper_t* ri_odb_create_anon_shm(ri_odb_t *odb);


ri_shm_mapper_t* ri_odb_create_named_shm(ri_odb_t *odb, const char *name, mode_t mode);

void ri_odb_dump(const ri_odb_t *odb);

ri_consumer_vector_t* ri_odb_get_consumer_vector(ri_odb_t *odb, unsigned channel_index);
ri_producer_vector_t* ri_odb_get_producer_vector(ri_odb_t *odb, unsigned channel_index);

const ri_object_meta_t* ri_consumer_vector_get(const ri_consumer_vector_t *consumer, unsigned index);
const ri_object_meta_t* ri_producer_vector_get(const ri_producer_vector_t *producer, unsigned index);

int ri_consumer_vector_add(ri_consumer_vector_t *consumer, const ri_object_meta_t *meta);
int ri_producer_vector_add(ri_producer_vector_t *producer, const ri_object_meta_t *meta);


int ri_odb_add_consumer_object(ri_odb_t *odb, unsigned channel_index, const ri_object_meta_t *meta);
int ri_odb_add_producer_object(ri_odb_t *odb, unsigned channel_index, const ri_object_meta_t *meta);

unsigned ri_consumer_vector_get_index(ri_consumer_vector_t *vec);

unsigned ri_producer_vector_get_index(ri_producer_vector_t *vec);

#ifdef __cplusplus
}
#endif
