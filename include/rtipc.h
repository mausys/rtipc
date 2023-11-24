#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @typedef ri_shm_t
 *
 * @brief shared memory
 */
typedef struct ri_shm ri_shm_t;


/**
 * @typedef ri_producer_t
 *
 * @brief writing to a shared memory channel
 */
typedef struct ri_producer ri_producer_t;

/**
 * @typedef ri_consumer_t
 *
 * @brief reading from a shared memory channel
 */
typedef struct ri_consumer ri_consumer_t;

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


/**
 * @typedef ri_object_t
 *
 * @brief data object, pointer will be mapped by consumer/producer on update
 */
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


/**
 * @brief ri_object_valid check if object description is valid
 *
 * @return true if valid
 */
static inline bool ri_object_valid(const ri_object_t *object)
{
    if (!object)
        return false;

    if (object->size == 0)
        return false;

    // single bit check
    if ((object->align == 0) || (object->align & (object->align - 1)))
        return false;

    return true;
}


/**
 * @brief ri_set_log_handler redirects rtipc library logs to custom handler
 *
 * @param log_handler function pointer to log handler
 */
void ri_set_log_handler(ri_log_fn log_handler);


/**
 * @brief ri_anon_shm_new creates, maps and initializes anonymous shared memory
 *        file descriptor can be retrieved with ri_shm_get_fd and send to client over an unix socket
 *        this function should only be called by server
 *
 * @param cns_sizes null terminated list of of consumer (server perspective) buffer sizes
 * @param prd_sizes null terminated list of of producer (server perspective) buffer sizes
 * @return pointer to the new shared memory object; NULL on error
 */
ri_shm_t* ri_anon_shm_new(const size_t cns_sizes[], const size_t prd_sizes[]);


/**
 * @brief ri_named_shm_new creates, maps and initializes named shared memory
 *        this function should only be called by server
 *
 * @param cns_sizes null terminated list of of consumer (server perspective) buffer sizes
 * @param prd_sizes null terminated list of of producer (server perspective) buffer sizes
 * @param name shared memory name (file system)
 * @param mode used by shm_open
 * @return pointer to the new shared memory object; NULL on error
 */
ri_shm_t* ri_named_shm_new(const size_t cns_sizes[], const size_t prd_sizes[], const char *name, mode_t mode);


/**
 * @brief ri_shm_map maps shared memory
 *        this function should only be called by client
 *
 * @param fd file descriptor of shared memory
 * @return pointer to the new shared memory object; NULL on error
 */
ri_shm_t* ri_shm_map(int fd);


/**
 * @brief ri_named_shm_map creates, maps named shared memory
 *        this function should only be called by client
 *
 * @param name shared memory name (file system)
 * @return pointer to the new shared memory object; NULL on error
 */
ri_shm_t* ri_named_shm_map(const char *name);


/**
 * @brief ri_shm_delete unmaeps and deletes shared memory and its channels
 *
 * @param shm shared memory object
 */
void ri_shm_delete(ri_shm_t *shm);


/**
 * @brief ri_shm_get_fd retreive file descriptor from shared memory object
 *
 * @param shm shared memory object
 * @return file descriptor
 */
int ri_shm_get_fd(const ri_shm_t *shm);


/**
 * @brief ri_shm_get_consumer get a pointer to a consumer
 *
 * @param shm shared memory object
 * @param cns_id consumer channel id
 * @return pointer to consumer; NULL on error
 */
ri_consumer_t* ri_shm_get_consumer(const ri_shm_t *shm, unsigned cns_id);


/**
 * @brief ri_shm_get_producer get a pointer to a producer
 *
 * @param shm shared memory object
 * @param cns_id producer channel id
 * @return pointer to producer; NULL on error
 */
ri_producer_t* ri_shm_get_producer(const ri_shm_t *shm, unsigned prd_id);


/**
 * @brief ri_consumer_fetch fetches a buffer from channel
 *
 * @param cns pointer to consumer
 * @return pointer to the latest buffer updated by the remote producer; NULL until remote producer updates it for the first time
 */
void* ri_consumer_fetch(ri_consumer_t *cns);


/**
 * @brief ri_producer_swap submits current buffer and get a new one for writing
 *
 * @param prd pointer to producer
 * @return pointer to buffer for writng
 */
void* ri_producer_swap(ri_producer_t *prd);

bool ri_producer_ackd(const ri_producer_t *prd);


/**
 * @brief ri_consumer_get_buffer_size submits current buffer and get a new one for writing
 *
 * @param prd cns to producer
 * @return size of buffer
 */
size_t ri_consumer_get_buffer_size(const ri_consumer_t *cns);

size_t ri_producer_get_buffer_size(const ri_producer_t *prd);

ri_shm_t* ri_objects_anon_shm_new(ri_object_t *c2s_objs[], ri_object_t *s2c_objs[]);


ri_shm_t* ri_objects_named_shm_new(ri_object_t *c2s_objs[], ri_object_t *s2c_objs[], const char *name, mode_t mode);

/**
 * @brief ri_calc_buffer_size calculates the total buffer size that is needed for containing
 * all the object in the object list
 *
 * @param objs object list, terminated with an entry with size=0
 * @return calculated buffer size
 */
size_t ri_calc_buffer_size(const ri_object_t objs[]);


/**
 * @brief ri_consumer_mapper_new creates a consumer object mapper
 *
 * @param shm shared memory
 * @param cns_id consumer id
 * @param objs object list, terminated with an entry with size=0
 * @return pointer to the new consumer object mapper; NULL on error
 */
ri_consumer_mapper_t* ri_consumer_mapper_new(ri_shm_t *shm, unsigned cns_id, const ri_object_t *objs);


/**
 * @brief ri_producer_mapper_new creates a producer object mapper
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
ri_producer_mapper_t* ri_producer_mapper_new(ri_shm_t *shm, unsigned prd_id, const ri_object_t *objs, bool cache);

void ri_consumer_mapper_delete(ri_consumer_mapper_t* cos);

void ri_producer_mapper_delete(ri_producer_mapper_t* pos);


/**
 * @brief ri_consumer_mapper_update swaps channel buffers and updates object pointers
 *
 * @param cos consumer object mapper
 * @retval -1 producer has not yet submit a buffer, object pointers are nullified
 * @retval 0 producer has not swapped buffers since last call, object pointers are staying the same
 * @retval 1 producer has swapped buffers since last call, object pointers are mapped to new buffer
 */
int ri_consumer_mapper_update(ri_consumer_mapper_t *cos);


/**
 * @brief ri_producer_mapper_update swaps channel buffers and updates object pointers
 *
 * @param pos producer object mapper
 */
void ri_producer_mapper_update(ri_producer_mapper_t *pos);


/**
 * @brief ri_producer_objects_ackd checks if consumer is using the latest buffer
 *
 * @param pos producer object mapper
 */
bool ri_producer_mapper_ackd(const ri_producer_mapper_t *pos);


#ifdef __cplusplus
}
#endif
