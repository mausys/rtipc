#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <rtipc/channel.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct ri_obj_desc {
    void *p; /**< actually this is a pointer to a pointer */
    size_t size;
    size_t align;
} ri_obj_desc_t;

/**
 * @typedef ri_rom_t
 *
 * @brief receive object mapper
 */
typedef struct ri_rom ri_rom_t;

/**
 * @typedef ri_tom_t
 *
 * @brief transmit object mapper
 */
typedef struct ri_tom ri_tom_t;

#define RI_OBJECT(x) (ri_obj_desc_t) { .p = &(x), .size = sizeof(*(x)), .align = __alignof__(*(x)) }
#define RI_OBJECT_ARRAY(x, s) (ri_obj_desc_t) { .p = &(x), .size = sizeof(*(x)) * (s), .align = __alignof__(*(x)) }
#define RI_OBJECT_NULL(s, a) (ri_obj_desc_t) { .p = NULL, .size = (s) , .align = a }
#define RI_OBJECT_END (ri_obj_desc_t) { .p = NULL, .size = 0 }


/**
 * @brief ri_calc_buffer_size calculates the total buffer size that is needed for containing
 * all the object in the object describtion list
 *
 * @param descs object describtion list, terminated with an entry with size=0
 * @return calculated buffer size
 */
size_t ri_calc_buffer_size(const ri_obj_desc_t descs[]);


/**
 * @brief ri_rom_new creates a receive object mapper
 *
 * @param chn receive channel that will be used by returned object mapper
 * @param descs object describtion list, terminated with an entry with size=0
 * @return pointer to the new receive object mapper; NULL on error
 */
ri_rom_t* ri_rom_new(const ri_rchn_t *chn, const ri_obj_desc_t *descs);


/**
 * @brief ri_tom_new creates a transmit object mapper
 *
 * @param chn transmit channel that will be used by returned object mapper
 * @param descs object describtion list, terminated with an entry with size=0
 * @param cache if true a buffer equally sized to the channel buffer is allocated
 * and the objects are statically mapped to this private buffer.
 * This cache will be copied to the channel buffer before swapping, so it is safe to read back the objects.
 * Otherwise the producer is responsible for updating all transmit objects before calling this function.
 * @return pointer to the new transmit object mapper; NULL on error
 */
ri_tom_t* ri_tom_new(const ri_tchn_t *chn, const ri_obj_desc_t *descs, bool cache);

void ri_rom_delete(ri_rom_t* rom);

void ri_tom_delete(ri_tom_t* tom);


/**
 * @brief ri_rom_update swaps channel buffers and updates object pointers
 *
 * @param rom receive object mapper
 * @retval -1 producer has not yet submit a buffer, object pointers are nullified
 * @retval 0 producer has not swapped buffers since last call, object pointers are staying the same
 * @retval 1 producer has swapped buffers since last call, object pointers are mapped to new buffer
 */
int ri_rom_update(ri_rom_t *rom);


/**
 * @brief ri_tom_update swaps channel buffers and updates object pointers
 *
 * @param tom transmit object mapper
 */
void ri_tom_update(ri_tom_t *tom);


/**
 * @brief ri_tom_ackd checks if consumer is using the latest buffer
 *
 * @param tom transmit object mapper
 */
bool ri_tom_ackd(const ri_tom_t *tom);


#ifdef __cplusplus
}
#endif
