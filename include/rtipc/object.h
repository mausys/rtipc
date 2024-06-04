#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

#define RI_OBJECT(x) (ri_object_meta_t) { .size = sizeof(x), .align = alignof(x) }
#define RI_OBJECT_ARRAY(x, n) (ri_object_meta_t) { .size = sizeof(x) * (n), .align = alignof(x) }
#define RI_OBJECT_ID(_id, x) (ri_object_meta_t) { .id = (_id), .size = sizeof(x), .align = alignof(x) }
#define RI_OBJECT_ARRAY_ID(_id, x, n) (ri_object_meta_t) { .id = (_id), .size = sizeof(x) * (n), .align = alignof(x) }
#define RI_OBJECT_END (ri_object_meta_t) { .size = 0, .align = 0 }


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

size_t ri_object_get_offset_next(size_t offset, const ri_object_meta_t *meta);

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


static inline bool ri_objects_equal(const ri_object_meta_t *meta1, const ri_object_meta_t *meta2)
{
    return (meta1->id == meta2->id) && (meta1->size == meta2->size) && (meta1->align == meta2->align);
}
