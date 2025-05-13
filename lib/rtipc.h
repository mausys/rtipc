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
 * @typedef ri_rtipc_t
 *
 * @brief ownes all channels and shm
 */
typedef struct ri_rtipc ri_rtipc_t;


/**
 * @typedef ri_channel_size_t
 *
 * @brief specifies channels size
 */
typedef struct ri_channel_size {
    uint32_t msg_size;
    uint32_t add_msgs; /* additional messages to the minimum of 3 */
} ri_channel_size_t;

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




typedef void (*ri_log_fn) (int priority, const char *file, const char *line,
                          const char *func, const char *format, va_list ap);

/**
 * @brief ri_set_log_handler redirects rtipc library logs to custom handler
 *
 * @param log_handler function pointer to log handler
 */
void ri_set_log_handler(ri_log_fn log_handler);

size_t ri_calc_shm_size(const ri_channel_size_t consumers[], const ri_channel_size_t producers[]);

/**
 * @brief ri_anon_shm_new creates, maps and initializes anonymous shared memory
 *        file descriptor can be retrieved with ri_shm_get_fd and send to client over an unix socket
 *        caller is owner of the shared memory
 *
 * @param consumers (msg_size = 0) terminated list of of consumers (owner perspective) channels
 * @param producers (msg_size = 0) terminated list of of producers (owner perspective) channels
 * @return pointer to the new rtipc object; NULL on error
 */
ri_rtipc_t* ri_rtipc_anon_shm_new(const ri_channel_size_t consumers[], const ri_channel_size_t producers[]);


/**
 * @brief ri_named_shm_new creates, maps and initializes named shared memory
 *        caller is owner of the shared memory
 *
 * @param consumers (msg_size = 0) terminated list of of consumers (owner perspective) channels
 * @param producers (msg_size = 0) terminated list of of producers (owner perspective) channels
 * @param name shared memory name (file system)
 * @param mode used by shm_open
 * @return pointer to the new shared memory object; NULL on error
 */
ri_rtipc_t* ri_rtipc_named_shm_new(const ri_channel_size_t consumers[], const ri_channel_size_t producers[], const char *name, mode_t mode);


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


unsigned ri_rtipc_get_num_consumers(const ri_rtipc_t *rtipc);
unsigned ri_rtipc_get_num_producers(const ri_rtipc_t *rtipc);

/**
 * @brief ri_rtipc_get_consumer get a pointer to a consumer
 *
 * @param shm shared memory object
 * @param index consumer channel index
 * @return pointer to consumer; NULL on error
 */
ri_consumer_t* ri_rtipc_get_consumer(const ri_rtipc_t *rtipc, unsigned index);


/**
 * @brief ri_rtipc_get_producer get a pointer to a producer
 *
 * @param shm shared memory object
 * @param index producer channel index
 * @return pointer to producer; NULL on error
 */
ri_producer_t* ri_rtipc_get_producer(const ri_rtipc_t *rtipc, unsigned index);


/**
 * @brief ri_rtipc_dump print shared memory information
 *
 * @param rtipc rtipc object
 */
void ri_rtipc_dump(const ri_rtipc_t *rtipc);

/**
 * @brief consumer_fetch_head fetches a buffer from channel
 *
 * @param consumer pointer to consumer
 * @return pointer to the latest message updated by the remote producer; NULL until remote producer updates it for the first time
 */
void* ri_consumer_fetch_head(ri_consumer_t *consumer);
void* ri_consumer_fetch_tail(ri_consumer_t *consumer);

void* ri_producer_get_msg(ri_producer_t *producer);
/**
 * @brief ri_produce submits current buffer and get a new one for writing
 *
 * @param producer pointer to producer
 * @return pointer to buffer for writng
 */
void* ri_producer_force_put(ri_producer_t *producer, bool *discarded);


void* ri_producer_try_put(ri_producer_t *producer);



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
