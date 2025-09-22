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
 * @typedef ri_rtipc_t
 *
 * @brief ownes all channels and shm
 */
typedef struct ri_rtipc ri_rtipc_t;

typedef struct ri_info {
  size_t size;
  const void *data;
} ri_info_t;

/**
 * @typedef ri_channel_size_t
 *
 * @brief specifies channels size
 */
typedef struct ri_channel_param
{
  size_t msg_size;
  unsigned add_msgs; /* additional messages to the minimum of 3 */
  bool eventfd;
  ri_info_t info;
} ri_channel_param_t;

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



typedef struct ri_consumer ri_consumer_t;

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
 * @brief ri_rtipc_shm_map maps shared memory
 *        retrieved from owner
 *
 * @param fd file descriptor of shared memory
 * @return pointer to the new rtipc memory object; NULL on error
 */
ri_rtipc_t* ri_rtipc_shm_map(int fd);

/**
 * @brief ri_rtipc_named_shm_map maps named shared memory
 *        retrieved from owner
 *
 * @param name shared memory name (file system)
 * @return pointer to the new rtipc object; NULL on error
 */
ri_rtipc_t* ri_rtipc_named_shm_map(const char *name);

/**
 * @brief ri_shm_delete unmaps and deletes shared memory and its channels
 *
 * @param rtipc object
 */
void ri_rtipc_delete(ri_rtipc_t *rtipc);

/**
 * @brief ri_rtipc_get_shm_fd retreive file descriptor from rtipc object
 *
 * @param rtipc object
 * @return file descriptor
 */
int ri_rtipc_get_shm_fd(const ri_rtipc_t *rtipc);

unsigned ri_rtipc_num_consumers(const ri_rtipc_t *rtipc);
unsigned ri_rtipc_num_producers(const ri_rtipc_t *rtipc);

/**
 * @brief ri_rtipc_take_consumer get a pointer to a consumer
 *
 * @param shm shared memory object
 * @param index consumer channel index
 * @return pointer to consumer; NULL on error
 */
ri_consumer_t* ri_rtipc_take_consumer(const ri_rtipc_t *rtipc, unsigned index);

/**
 * @brief ri_rtipc_take_producer get a pointer to a producer
 *
 * @param shm shared memory object
 * @param index producer channel index
 * @return pointer to producer; NULL on error
 */
ri_producer_t* ri_rtipc_take_producer(const ri_rtipc_t *rtipc, unsigned index);


/**
 * @brief ri_rtipc_dump print shared memory information
 *
 * @param rtipc rtipc object
 */
void ri_rtipc_dump(const ri_rtipc_t *rtipc);

/**
 * @brief ri_consumer_msg get pointer to current message
 *
 * @param consumer pointer to consumer
 * @return pointer to current message (always valid)
 */
const void* ri_consumer_msg(ri_consumer_t *consumer);

/**
 * @brief consumer_flush get message from the head, discarding all older messages
 *
 * @param consumer pointer to consumer
 * @return pointer to the latest message updated by the remote producer; NULL until remote producer updates it for the first time
 */
ri_consume_result_t ri_consumer_flush(ri_consumer_t *consumer);
ri_consume_result_t ri_consumer_pop(ri_consumer_t *consumer);

/**
 * @brief ri_producer_msg get pointer to current message
 *
 * @param producer pointer to producer
 * @return pointer to current message (always valid)
 */
void* ri_producer_msg(ri_producer_t *producer);

/**
 * @brief ri_producer_force_put submits current message and get a new message
 *
 * @param producer pointer to producer
 * @return 0 => success, 1 => success, but discarded last unused message
 */
ri_produce_result_t ri_producer_force_push(ri_producer_t *producer);

/**
 * @brief ri_producer_try_put submits current message and get a new message,
 * if queue is not full
 *
 * @param producer pointer to producer
 * @return 0 => success, -1 => fail, because queue was full
 */
ri_produce_result_t ri_producer_try_push(ri_producer_t *producer);

/**
 * @brief ri_consumer_get_buffer_size submits current buffer and get a new one for writing
 *
 * @param producer pointer to producer
 * @return size of buffer
 */
size_t ri_consumer_msg_size(const ri_consumer_t *consumer);

size_t ri_producer_msg_size(const ri_producer_t *producer);

void ri_consumer_delete(ri_consumer_t *consumer);
void ri_producer_delete(ri_producer_t *producer);


ri_info_t ri_consumer_info(const ri_consumer_t *consumer);
ri_info_t ri_producer_info(const ri_producer_t *producer);


void ri_consumer_free_info(ri_consumer_t *consumer);
void ri_producer_free_info(ri_producer_t *producer);

#ifdef __cplusplus
}
#endif
