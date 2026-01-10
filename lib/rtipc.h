#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @typedef ri_vector_t
 *
 * @brief producers/consumers mapped on the same shared memory
 */
typedef struct ri_vector ri_vector_t;

typedef bool (*ri_filter_fn)(const ri_vector_t* vec, void *user_data);


typedef struct ri_info {
  size_t size;
  const void *data;
} ri_info_t;

/**
 * @typedef ri_channel_config_t
 *
 * @brief all paramteters needed for creating a channel
 */
typedef struct ri_channel_config {
  size_t msg_size;
  unsigned add_msgs; /* additional messages to the minimum length of 3 */
  int eventfd; /* eventfd for notifying the consumer when the producer added a message */
  ri_info_t info; /* user defined info will be transmitted to the server */
} ri_channel_config_t;

/**
 * @typedef ri_vector_config_t
 *
 * @brief all paramteters needed for creating a vector
 */
typedef struct ri_vector_config {
  const ri_channel_config_t *consumers; /* 0 terminated (msg_size = 0) list of consumers */
  const ri_channel_config_t *producers; /* 0 terminated (msg_size = 0) list of producers */
  ri_info_t info;
} ri_vector_config_t;


/**
 * @typedef ri_vector_transfer_t
 *
 * @brief all paramteters needed for creating a vector
 */
typedef struct ri_vector_transfer {
  ri_channel_config_t *consumers; /* 0 terminated (msg_size = 0) list of consumers */
  ri_channel_config_t *producers; /* 0 terminated (msg_size = 0) list of producers */
  ri_info_t info;
  int shmfd;
} ri_vector_transfer_t;

/**
 * @typedef ri_server_t
 *
 * @brief server for unix socket
 */
typedef struct ri_server ri_server_t;

/**
 * @typedef ri_consumer_t
 *
 * @brief reading from a shared memory channel
 */
typedef struct ri_consumer ri_consumer_t;

/**
 * @typedef ri_producer_t
 *
 * @brief writing to a shared memory channel
 */
typedef struct ri_producer ri_producer_t;


typedef void (*ri_log_fn)(int priority,
                          const char *file,
                          const char *line,
                          const char *func,
                          const char *format,
                          va_list ap);

typedef enum ri_consume_result {
  RI_CONSUME_RESULT_ERROR = -2,
  RI_CONSUME_RESULT_NO_MSG = -1,
  RI_CONSUME_RESULT_NO_UPDATE = 0,
  RI_CONSUME_RESULT_SUCCESS = 1,
  RI_CONSUME_RESULT_DISCARDED = 2,
} ri_consume_result_t;

typedef enum ri_produce_result {
  RI_PRODUCE_RESULT_ERROR = -2,
  RI_PRODUCE_RESULT_FAIL = -1,
  RI_PRODUCE_RESULT_SUCCESS = 1,
  RI_PRODUCE_RESULT_DISCARDED = 2,
} ri_produce_result_t;

/**
 * @brief ri_set_log_handler redirects rtipc library logs to custom handler
 *
 * @param log_handler function pointer to log handler
 */
void ri_set_log_handler(ri_log_fn log_handler);


/**
 * @brief ri_server_new creates a server socket and starts listening
 *
 * @param path pathname of socket
 * @param backlog argument for listen
 *
 */
ri_server_t* ri_server_new(const char* path, int backlog);

/**
 * @brief ri_server_socket
 * @param server pointer to server
 * @return sockfd
 */
int ri_server_socket(const ri_server_t* server);

/**
 * @brief ri_server_accept accepts a connection from client and builds a channel vector
 * @param server
 * @param filter user function for accepting (return true) or rejecting (return false) request
 *  NULL for accepting all
 * @param userdata passed to filter
 * @return channel vector
 */
ri_vector_t* ri_server_accept(const ri_server_t* server, ri_filter_fn filter, void *user_data);


/**
 * @brief ri_server_delete delete server
 * @param server
 */
void ri_server_delete(ri_server_t* server);

ri_vector_t* ri_client_connect(const char *path, const ri_vector_config_t *vconfig);
ri_vector_transfer_t* ri_vector_transfer_new(unsigned n_consumers, unsigned n_producers, const ri_info_t *info);
void ri_vector_transfer_delete(ri_vector_transfer_t* vxfer);
ri_vector_t* ri_vector_new(const ri_vector_config_t *vconfig);
ri_vector_t* ri_vector_map(ri_vector_transfer_t *vxfer);
void ri_vector_delete(ri_vector_t *vec);

unsigned ri_vector_num_consumers(const ri_vector_t *vec);

unsigned ri_vector_num_producers(const ri_vector_t *vec);



/**
 * @brief ri_vector_take_consumer get a pointer to a consumer
 *
 * @param shm shared memory object
 * @param index consumer channel index
 * @return pointer to consumer; NULL on error
 */
ri_consumer_t* ri_vector_take_consumer(ri_vector_t *vec, unsigned index);

/**
 * @brief ri_rtipc_take_producer get a pointer to a producer
 *
 * @param shm shared memory object
 * @param index producer channel index
 * @return pointer to producer; NULL on error
 */
ri_producer_t* ri_vector_take_producer(ri_vector_t *vec, unsigned index);

/**
 * @brief ri_consumer_delete deletes channel; if its the last channel of the vector, it will also delete the shared memory
 * @param consumer pointer to consumer, invalid after call
 */
void ri_consumer_delete(ri_consumer_t *consumer);

/**
 * @brief ri_consumer_msg get pointer to current message
 *
 * @param consumer pointer to consumer
 * @return pointer to current message (always valid)
 */
const void* ri_consumer_msg(const ri_consumer_t *consumer);

/**
 * @brief ri_consumer_flush get message from the head, discarding all older messages
 *
 * @param consumer pointer to consumer
 * @return result
 */
ri_consume_result_t ri_consumer_flush(ri_consumer_t *consumer);


/**
 * @brief ri_consumer_pop take oldest message from the queue
 *
 * @param consumer pointer to consumer
 * @return result
 */
ri_consume_result_t ri_consumer_pop(ri_consumer_t *consumer);

ri_channel_config_t ri_consumer_config(const ri_consumer_t *consumer);


/**
 * @brief ri_consumer_msg_size get message size
 *
 * @param consumer pointer to consumer
 * @return size of message
 */
size_t ri_consumer_msg_size(const ri_consumer_t *consumer);
\

/**
 * @brief ri_consumer_eventfd get eventfd, but consumer still uses eventfd and closes it on deleteion
 *
 * @param consumer pointer to consumer
 * @return eventfd
 */
int ri_consumer_eventfd(const ri_consumer_t *consumer);


/**
 * @brief ri_consumer_eventfd take eventfd, consumer has no more access to eventfd
 *
 * @param consumer pointer to consumer
 * @return eventfd
 */
int ri_consumer_take_eventfd(ri_consumer_t *consumer);


/**
 * @brief ri_consumer_info git channel info
 * @param consumer pointr to consumer
 * @return channel info
 */
ri_info_t ri_consumer_info(const ri_consumer_t *consumer);

/**
 * @brief ri_consumer_free_info deletes info
 *
 * @param consumer pointer to consumer
 */
void ri_consumer_free_info(ri_consumer_t *consumer);

/**
 * @brief ri_producer_delete deletes channel; if its the last channel of the vector, it will also delete the shared memory
 * @param producer pointer to producer, invalid after call
 */
void ri_producer_delete(ri_producer_t *producer);

/**
 * @brief ri_producer_msg get pointer to current message
 *
 * @param producer pointer to producer
 * @return pointer to current message (always valid)
 */
void* ri_producer_msg(const ri_producer_t *producer);

/**
 * @brief ri_producer_force_push submits current message and get a new message
 *
 * @param producer pointer to producer
 * @return result
 */
ri_produce_result_t ri_producer_force_push(ri_producer_t *producer);

/**
 * @brief ri_producer_try_push submits current message and get a new message,
 * if queue is not full
 *
 * @param producer pointer to producer
 * @return result
 */
ri_produce_result_t ri_producer_try_push(ri_producer_t *producer);


ri_channel_config_t ri_producer_config(const ri_producer_t *producer);

/**
 * @brief ri_producer_msg_size get message size
 *
 * @param producer pointer to producer
 * @return size of message
 */
size_t ri_producer_msg_size(const ri_producer_t *producer);


/**
 * @brief ri_producer_eventfd get eventfd, but producer still uses eventfd and closes it on deleteion
 *
 * @param producer pointer to producer
 * @return eventfd
 */
int ri_producer_eventfd(const ri_producer_t *producer);

/**
 * @brief ri_producer_take_eventfd take eventfd, producer has no more access to eventfd
 * @param producer pointer to producer
 * @return eventfd
 */
int ri_producer_take_eventfd(ri_producer_t *producer);

/**
 * @brief ri_producer_cache_enable enables message cache and copies current message to cache.
 *  ri_producer_msg will always return pointer to cache. Cache is written back with push
 * @param producer pointer to producer
 * @return 0 on success
 */
int ri_producer_cache_enable(ri_producer_t *producer);

/**
 * @brief ri_producer_cache_disable if cache was enabled, copies message cache to current message
 * and deletes message cache.
 * ri_producer_msg will return pointer to current message (zero copy)
 * @param producer pointer to producer
 */
void ri_producer_cache_disable(ri_producer_t *producer);

/**
 * @brief ri_producer_info get channel info
 * @param producer pointer to producer
 * @return channel info
 */
ri_info_t ri_producer_info(const ri_producer_t *producer);

/**
 * @brief ri_producer_free_info deletes info
 * @param producer pointer to producer
 */
void ri_producer_free_info(ri_producer_t *producer);


ri_vector_transfer_t* ri_vector_transfer_new(unsigned n_consumers, unsigned n_producers, const ri_info_t *info);
void ri_vector_transfer_delete(ri_vector_transfer_t *vxfer);


ri_info_t ri_vector_info(const ri_vector_t *vec);

const ri_producer_t* ri_vector_get_producer(const ri_vector_t *vec, unsigned index);


const ri_consumer_t* ri_vector_get_consumer(const ri_vector_t *vec, unsigned index);


size_t ri_request_calc_size(const ri_vector_config_t *vconfig);
ri_vector_transfer_t* ri_request_parse(const void *req, size_t size);
int ri_request_write(const ri_vector_config_t* vconfig, void *req, size_t size);
int ri_vector_get_shmfd(const ri_vector_t *vec);

#ifdef __cplusplus
}
#endif
