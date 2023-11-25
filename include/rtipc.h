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

typedef struct ri_span {
    const void *ptr;
    size_t size;
} ri_span_t;

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

typedef struct ri_channel_description {
    uint32_t buffer_size;
    ri_span_t meta;
} ri_channel_description_t;

typedef struct ri_shm_mapper ri_shm_mapper_t;

/**
 * @typedef ri_object_id_t
 *
 * @brief used by opject mapper to indentify objects
 */
typedef uint64_t ri_object_id_t;


/**
 * @typedef ri_object_meta
 *
 * @brief object meta data
 */
typedef struct ri_object_meta {
    ri_object_id_t id;
    uint32_t size;
    uint8_t align;
} ri_object_meta_t;


/**
 * @typedef ri_object_t
 *
 * @brief data object, pointer will be mapped by consumer/producer on update
 */
typedef struct ri_object {
    ri_object_meta_t meta;
    void *ptr; /**< actually this is a pointer to a pointer */
} ri_object_t;


#define RI_OBJECT(x) (ri_object_t) { .meta.size = sizeof(*(x)), .meta.align = __alignof__(*(x)), .ptr = &(x) }
#define RI_OBJECT_ARRAY(x, s) (ri_object_t) { .meta.size = sizeof(*(x)) * (s), .meta.align = __alignof__(*(x)), .ptr = &(x) }
#define RI_OBJECT_ID(_id, x) (ri_object_t) { .meta.id = (_id), .meta.size = sizeof(*(x)), .meta.align = __alignof__(*(x)), .ptr = &(x) }
#define RI_OBJECT_ARRAY_ID(_id, x, s) (ri_object_t) { .meta.id = (_id), .meta.size = sizeof(*(x)) * (s), .meta.align = __alignof__(*(x)), .ptr = &(x) }
#define RI_OBJECT_NULL(s, a) (ri_object_t) { .meta.size = (s) , .meta.align = a, .ptr = NULL}
#define RI_OBJECT_END (ri_object_t) { .meta.size = 0, .meta.align = 0, .ptr = NULL}

typedef void (*ri_log_fn) (int priority, const char *file, const char *line,
                          const char *func, const char *format, va_list ap);

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
 * @param cns desc terminated list of of consumer (server perspective) channel descriptions
 * @param prd_descs null terminated list of of producer (server perspective) descriptions
 * @return pointer to the new shared memory object; NULL on error
 */
ri_shm_t* ri_anon_shm_new(const ri_channel_description_t cns_descs[], const ri_channel_description_t prd_descs[]);


/**
 * @brief ri_named_shm_new creates, maps and initializes named shared memory
 *        this function should only be called by server
 *
 * @param cns desc terminated list of of consumer (server perspective) channel descriptions
 * @param prd_descs null terminated list of of producer (server perspective) descriptions
 * @param name shared memory name (file system)
 * @param mode used by shm_open
 * @return pointer to the new shared memory object; NULL on error
 */
ri_shm_t* ri_named_shm_new(const ri_channel_description_t cns_descs[], const ri_channel_description_t prd_descs[], const char *name, mode_t mode);


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


unsigned ri_shm_get_num_consumers(const ri_shm_t *shm);
unsigned ri_shm_get_num_producers(const ri_shm_t *shm);

/**
 * @brief ri_shm_get_consumer get a pointer to a consumer
 *
 * @param shm shared memory object
 * @param index consumer channel index
 * @return pointer to consumer; NULL on error
 */
ri_consumer_t* ri_shm_get_consumer(const ri_shm_t *shm, unsigned index);


/**
 * @brief ri_shm_get_producer get a pointer to a producer
 *
 * @param shm shared memory object
 * @param index producer channel index
 * @return pointer to producer; NULL on error
 */
ri_producer_t* ri_shm_get_producer(const ri_shm_t *shm, unsigned index);


/**
 * @brief ri_shm_dump print shared memory information
 *
 * @param shm shared memory object
 */
void ri_shm_dump(const ri_shm_t *shm);

ri_span_t ri_producer_get_meta(const ri_producer_t *producer);

ri_span_t ri_consumer_get_meta(const ri_consumer_t *consumer);

/**
 * @brief ri_consumer_fetch fetches a buffer from channel
 *
 * @param consumer pointer to consumer
 * @return pointer to the latest buffer updated by the remote producer; NULL until remote producer updates it for the first time
 */
void* ri_consumer_fetch(ri_consumer_t *consumer);


/**
 * @brief ri_producer_swap submits current buffer and get a new one for writing
 *
 * @param producer pointer to producer
 * @return pointer to buffer for writng
 */
void* ri_producer_swap(ri_producer_t *producer);

bool ri_producer_ackd(const ri_producer_t *producer);


/**
 * @brief ri_consumer_get_buffer_size submits current buffer and get a new one for writing
 *
 * @param producer pointer to producer
 * @return size of buffer
 */
size_t ri_consumer_get_buffer_size(const ri_consumer_t *consumer);

size_t ri_producer_get_buffer_size(const ri_producer_t *producer);





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
 * @brief ri_object_valid check if object description is valid
 *
 * @return true if valid
 */
static inline bool ri_object_valid(const ri_object_meta_t *meta)
{
    if (!meta)
        return false;

    if (meta->size == 0)
        return false;

    // single bit check
    if ((meta->align == 0) || (meta->align & (meta->align - 1)))
        return false;

    return true;
}


ri_shm_mapper_t* ri_server_anon_shm_mapper_new(ri_object_t *consumer_channels[], ri_object_t *producer_channels[]);


ri_shm_mapper_t* ri_server_named_shm_mapper_new(ri_object_t *consumer_channels[], ri_object_t *producer_channels[], const char *name, mode_t mode);

ri_shm_mapper_t* ri_client_shm_mapper_new(int fd, ri_object_t *consumer_channels[], ri_object_t *producer_channels[]);

ri_shm_mapper_t* ri_client_named_shm_mapper_new(const char *name, ri_object_t *consumer_channels[], ri_object_t *producer_channels[]);

ri_shm_t* ri_shm_mapper_get_shm(const ri_shm_mapper_t* shm_mapper);

/**
 * @brief ri_calc_buffer_size calculates the total buffer size that is needed for containing
 * all the object in the object list
 *
 * @param objs object list, terminated with an entry with size=0
 * @return calculated buffer size
 */
size_t ri_calc_buffer_size(const ri_object_t objects[]);

void ri_shm_mapper_delete(ri_shm_mapper_t *shm_mapper);


ri_consumer_mapper_t* ri_shm_mapper_get_consumer(ri_shm_mapper_t *shm_mapper, unsigned index);

ri_producer_mapper_t* ri_shm_mapper_get_producer(ri_shm_mapper_t *shm_mapper, unsigned index);

void ri_producer_mapper_assign(ri_producer_mapper_t *mapper, const ri_object_t objects[]);
void ri_consumer_mapper_assign(ri_consumer_mapper_t *mapper, const ri_object_t objects[]);

int ri_producer_mapper_enable_cache(ri_producer_mapper_t *mapper);

void ri_producer_mapper_disable_cache(ri_producer_mapper_t *mapper);

void ri_producer_mapper_update(ri_producer_mapper_t *mapper);
int ri_consumer_mapper_update(ri_consumer_mapper_t *mapper);

ri_consumer_t* ri_consumer_get_channel(ri_consumer_mapper_t *mapper);

ri_producer_t* ri_producer_get_channel(ri_producer_mapper_t *mapper);

void ri_producer_mapper_dump(const ri_producer_mapper_t* mapper);
void ri_consumer_mapper_dump(const ri_consumer_mapper_t *mapper);


#ifdef __cplusplus
}
#endif
