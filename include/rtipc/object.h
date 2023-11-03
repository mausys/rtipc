#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <rtipc/channel.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct ri_object {
    void *p; /**< actually this is a pointer to a pointer */
    size_t size;
    size_t align;
} ri_object_t;

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

#define RI_OBJECT(x) (ri_object_t) { .p = &(x), .size = sizeof(*(x)), .align = __alignof__(*(x)) }
#define RI_OBJECT_ARRAY(x, s) (ri_object_t) { .p = &(x), .size = sizeof(*(x)) * (s), .align = __alignof__(*(x)) }
#define RI_OBJECT_NULL(s, a) (ri_object_t) { .p = NULL, .size = (s) , .align = a }
#define RI_OBJECT_END (ri_object_t) { .p = NULL, .size = 0, .align = 0 }


/**
 * @brief ri_calc_buffer_size calculates the total buffer size that is needed for containing
 * all the object in the object list
 *
 * @param objs object list, terminated with an entry with size=0
 * @return calculated buffer size
 */
size_t ri_calc_buffer_size(const ri_object_t objs[]);


/**
 * @brief ri_consumer_objects_new creates a receive object mapper
 *
 * @param chn receive channel that will be used by returned object mapper
 * @param objs object list, terminated with an entry with size=0
 * @return pointer to the new receive object mapper; NULL on error
 */
ri_consumer_objects_t* ri_consumer_objects_new(const ri_consumer_t *cns, const ri_object_t *objs);


/**
 * @brief ri_tom_new creates a transmit object mapper
 *
 * @param chn transmit channel that will be used by returned object mapper
 * @param objs object list, terminated with an entry with size=0
 * @param cache if true a buffer equally sized to the channel buffer is allocated
 * and the objects are statically mapped to this private buffer.
 * This cache will be copied to the channel buffer before swapping, so it is safe to read back the objects.
 * Otherwise the producer is responsible for updating all transmit objects before calling this function.
 * @return pointer to the new transmit object mapper; NULL on error
 */
ri_producer_objects_t* ri_producer_objects_new(const ri_producer_t *prd, const ri_object_t *objs, bool cache);

void ri_consumer_objects_delete(ri_consumer_objects_t* cos);

void ri_producer_objects_delete(ri_producer_objects_t* pos);


/**
 * @brief ri_consumer_objects_update swaps channel buffers and updates object pointers
 *
 * @param cos receive object mapper
 * @retval -1 producer has not yet submit a buffer, object pointers are nullified
 * @retval 0 producer has not swapped buffers since last call, object pointers are staying the same
 * @retval 1 producer has swapped buffers since last call, object pointers are mapped to new buffer
 */
int ri_consumer_objects_update(ri_consumer_objects_t *cos);


/**
 * @brief ri_producer_objects_update swaps channel buffers and updates object pointers
 *
 * @param pos transmit object mapper
 */
void ri_producer_objects_update(ri_producer_objects_t *pos);


/**
 * @brief ri_producer_objects_ackd checks if consumer is using the latest buffer
 *
 * @param pos transmit object mapper
 */
bool ri_producer_objects_ackd(const ri_producer_objects_t *pos);


#ifdef __cplusplus
}
#endif
