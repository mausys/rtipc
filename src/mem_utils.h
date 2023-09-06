#pragma once

#include <stddef.h>
#include <stdint.h>

static inline size_t mem_align(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}


static inline void* mem_offset(void *base, size_t offset)
{
    return (void*)((uintptr_t)base + offset);
}


size_t cache_line_size(void);
