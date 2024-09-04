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

typedef struct ri_channel_req {
    uint32_t buffer_size;
    ri_span_t meta;
} ri_channel_req_t;




typedef void (*ri_log_fn) (int priority, const char *file, const char *line,
                          const char *func, const char *format, va_list ap);

/**
 * @brief ri_set_log_handler redirects rtipc library logs to custom handler
 *
 * @param log_handler function pointer to log handler
 */
void ri_set_log_handler(ri_log_fn log_handler);

/**
 * @brief ri_shm_get_meta creates, get the shared memory meta data
 *
 * @param shm shared memory object
 */
ri_span_t ri_shm_get_meta(const ri_shm_t *shm);

/**
 * @brief ri_anon_shm_new creates, maps and initializes anonymous shared memory
 *        file descriptor can be retrieved with ri_shm_get_fd and send to client over an unix socket
 *        this function should only be called by server
 *
 * @param consumers terminated list of of consumer (server perspective) channel descriptions
 * @param producers null terminated list of of producer (server perspective) descriptions
 * @param shm_meta shared memory meta data
 * @return pointer to the new shared memory object; NULL on error
 */
ri_shm_t* ri_anon_shm_new(const ri_channel_req_t consumers[], const ri_channel_req_t producers[], const ri_span_t *shm_meta);


/**
 * @brief ri_named_shm_new creates, maps and initializes named shared memory
 *        this function should only be called by server
 *
 * @param cns consumers terminated list of of consumer (server perspective) channel descriptions
 * @param producers null terminated list of of producer (server perspective) descriptions
 * @param shm_meta shared memory meta data
 * @param name shared memory name (file system)
 * @param mode used by shm_open
 * @return pointer to the new shared memory object; NULL on error
 */
ri_shm_t* ri_named_shm_new(const ri_channel_req_t consumers[], const ri_channel_req_t producers[], const ri_span_t *shm_meta, const char *name, mode_t mode);


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

bool ri_producer_consumed(const ri_producer_t *producer);


/**
 * @brief ri_consumer_get_buffer_size submits current buffer and get a new one for writing
 *
 * @param producer pointer to producer
 * @return size of buffer
 */
size_t ri_consumer_get_buffer_size(const ri_consumer_t *consumer);

size_t ri_producer_get_buffer_size(const ri_producer_t *producer);




#ifdef __cplusplus
}
#endif
