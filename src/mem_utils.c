#include "mem_utils.h"

#include <unistd.h>
#include <stdatomic.h>

#include "log.h"

#define MIN_CACHE_LINE_SIZE 0x10
#define MAX_SANE_CACHE_LINE_SIZE 0x1000


static size_t get_cls_level(int level, size_t min)
{
    long r = sysconf(level);

    if (r < 0)
        return min;

    size_t size = r;

    // check for single bit
    if (!size || (size & (size - 1)))
        return min;

    if (size > MAX_SANE_CACHE_LINE_SIZE)
        return min;

    return size;
}


size_t cache_line_size(void)
{
    static atomic_ulong s_cls = 0;
    size_t cls = atomic_load_explicit(&s_cls, memory_order_relaxed);

    if (cls != 0)
        return cls;

    cls = get_cls_level(_SC_LEVEL1_DCACHE_LINESIZE, MIN_CACHE_LINE_SIZE);
    cls = get_cls_level(_SC_LEVEL2_CACHE_LINESIZE, cls);
    cls = get_cls_level(_SC_LEVEL3_CACHE_LINESIZE, cls);

    atomic_store_explicit(&s_cls, cls, memory_order_relaxed);

    LOG_INF("cache_line_size=%zu", cls);

    return cls;
}
