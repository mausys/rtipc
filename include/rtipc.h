#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct ri_shm ri_shm_t;

typedef struct ri_producer ri_producer_t;

typedef struct ri_consumer ri_consumer_t;

/**
 * @typedef ri_consumer_objects_t
 *
 * @brief consumer object mapper
 */
typedef struct ri_consumer_objects ri_consumer_objects_t;

/**
 * @typedef ri_producer_objects_t
 *
 * @brief producer object mapper
 */
typedef struct ri_producer_objects ri_producer_objects_t;


typedef struct ri_object {
    void *p; /**< actually this is a pointer to a pointer */
    size_t size;
    size_t align;
} ri_object_t;

typedef void (*ri_log_fn) (int priority, const char *file, const char *line,
                          const char *func, const char *format, va_list ap);


#define RI_OBJECT(x) (ri_object_t) { .p = &(x), .size = sizeof(*(x)), .align = __alignof__(*(x)) }
#define RI_OBJECT_ARRAY(x, s) (ri_object_t) { .p = &(x), .size = sizeof(*(x)) * (s), .align = __alignof__(*(x)) }
#define RI_OBJECT_NULL(s, a) (ri_object_t) { .p = NULL, .size = (s) , .align = a }
#define RI_OBJECT_END (ri_object_t) { .p = NULL, .size = 0, .align = 0 }


void ri_set_log_handler(ri_log_fn log_handler);

ri_shm_t* ri_anon_shm_new(const size_t c2s_chns[], const size_t s2c_chns[]);

ri_shm_t* ri_named_shm_new(const size_t c2s_chns[], const size_t s2c_chns[], const char *name, mode_t mode);

ri_shm_t* ri_shm_map(int fd);

ri_shm_t* ri_named_shm_map(const char *name);

void ri_shm_delete(ri_shm_t *shm);

int ri_shm_get_fd(const ri_shm_t *shm);

ri_consumer_t* ri_shm_get_consumer(const ri_shm_t *shm, unsigned cns_id);

ri_producer_t* ri_shm_get_producer(const ri_shm_t *shm, unsigned prd_id);

void* ri_consumer_fetch(ri_consumer_t *cns);

void* ri_producer_swap(ri_producer_t *prd);

bool ri_producer_ackd(const ri_producer_t *prd);

size_t ri_consumer_get_buffer_size(const ri_consumer_t *cns);

size_t ri_producer_get_buffer_size(const ri_producer_t *prd);

ri_shm_t* ri_objects_anon_shm_new(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[]);


ri_shm_t* ri_objects_named_shm_new(const ri_object_t *c2s_objs[], const ri_object_t *s2c_objs[], const char *name, mode_t mode);

/**
 * @brief ri_calc_buffer_size calculates the total buffer size that is needed for containing
 * all the object in the object list
 *
 * @param objs object list, terminated with an entry with size=0
 * @return calculated buffer size
 */
size_t ri_calc_buffer_size(const ri_object_t objs[]);


/**
 * @brief ri_consumer_objects_new creates a consumer object mapper
 *
 * @param shm shared memory
 * @param cns_id consumer id
 * @param objs object list, terminated with an entry with size=0
 * @return pointer to the new consumer object mapper; NULL on error
 */
ri_consumer_objects_t* ri_consumer_objects_new(ri_shm_t *shm, unsigned cns_id, const ri_object_t *objs);


/**
 * @brief ri_producer_objects_new creates a producer object mapper
 *
 * @param shm shared memory
 * @param prd_id producer id
 * @param objs object list, terminated with an entry with size=0
 * @param cache if true a buffer equally sized to the channel buffer is allocated
 * and the objects are statically mapped to this private buffer.
 * This cache will be copied to the channel buffer before swapping, so it is safe to read back the objects.
 * Otherwise the producer is responsible for updating all producer objects before calling this function.
 * @return pointer to the new producer object mapper; NULL on error
 */
ri_producer_objects_t* ri_producer_objects_new(ri_shm_t *shm, unsigned prd_id, const ri_object_t *objs, bool cache);

void ri_consumer_objects_delete(ri_consumer_objects_t* cos);

void ri_producer_objects_delete(ri_producer_objects_t* pos);


/**
 * @brief ri_consumer_objects_update swaps channel buffers and updates object pointers
 *
 * @param cos consumer object mapper
 * @retval -1 producer has not yet submit a buffer, object pointers are nullified
 * @retval 0 producer has not swapped buffers since last call, object pointers are staying the same
 * @retval 1 producer has swapped buffers since last call, object pointers are mapped to new buffer
 */
int ri_consumer_objects_update(ri_consumer_objects_t *cos);


/**
 * @brief ri_producer_objects_update swaps channel buffers and updates object pointers
 *
 * @param pos producer object mapper
 */
void ri_producer_objects_update(ri_producer_objects_t *pos);


/**
 * @brief ri_producer_objects_ackd checks if consumer is using the latest buffer
 *
 * @param pos producer object mapper
 */
bool ri_producer_objects_ackd(const ri_producer_objects_t *pos);





#ifdef __cplusplus
}
#endif
