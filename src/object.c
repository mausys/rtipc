#include "rtipc/object.h"

#include "mem_utils.h"


size_t ri_object_get_offset_next(size_t offset, const ri_object_meta_t *meta)
{
    if (meta->align != 0)
        offset = mem_align(offset, meta->align);

    return offset + meta->size;
}

