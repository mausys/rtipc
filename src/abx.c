#include "rtipc/abx.h"

#define _GNU_SOURCE

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/mman.h> // memfd_create

#include "mem_utils.h"

#if ATOMIC_INT_LOCK_FREE == 2
typedef atomic_uint xchg_t;
#elif ATOMIC_SHORT_LOCK_FREE == 2
typedef atomic_ushort xchg_t;
#elif ATOMIC_CHAR_LOCK_FREE == 2
typedef atomic_uchar xchg_t;
#else
#warning "no suitable always lockfree datatype found"
typedef atomic_uint xchg_t;
#endif


#define MIN_CACHE_LINE_SIZE 0x10
#define MAX_SANE_CACHE_LINE_SIZE 0x1000

#define NUM_BUFFERS 3

#define LOCK_FLAG 0x80


typedef enum {
    BUFFER_0 = 0,
    BUFFER_1,
    BUFFER_2,
    BUFFER_NONE,
} buffer_idx_t;


typedef struct {
    size_t buffer_size;
    xchg_t *xchg;
    void *buffers[NUM_BUFFERS];
} channel_t;


typedef struct {
    void *base;
    size_t size;
    int fd;
} shm_t;


struct abx {
    bool owner;
    size_t alignment;
    shm_t shm;
    struct {
        channel_t channel;
        buffer_idx_t current;
        buffer_idx_t locked;
    } tx;
    struct {
        channel_t channel;
    } rx;
};





static int create_shm(size_t size)
{
    static atomic_uint anr = 0;

    unsigned nr = atomic_fetch_add_explicit(&anr, 1, memory_order_relaxed);

    char name[64];

    snprintf(name, sizeof(name) - 1, "abx_%u", nr);

    int r = memfd_create(name, MFD_ALLOW_SEALING);

    if (r < 0)
        return -errno;

    int fd = r;

    r = ftruncate(fd, size);

    if (r < 0) {
        r = -errno;
        goto fail;
    }

    r = fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL);

    if (r < 0) {
        r = -errno;
        goto fail;
    }

    return fd;

fail:
    close(fd);
    return r;
}


static size_t get_cache_line_size(int level, size_t min)
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


static size_t get_alignment(void)
{
    size_t alignment = get_cache_line_size(_SC_LEVEL1_DCACHE_LINESIZE, MIN_CACHE_LINE_SIZE);
    alignment = get_cache_line_size(_SC_LEVEL2_CACHE_LINESIZE, alignment);
    return get_cache_line_size(_SC_LEVEL3_CACHE_LINESIZE, alignment);
}




static size_t get_header_size(const abx_t *abx)
{
    return mem_align(2 * sizeof(xchg_t), abx->alignment);
}


static size_t map_channel(channel_t *channel, void *base, void *xchg, size_t offset)
{
    if (channel->buffer_size == 0)
        return offset;

    channel->xchg = xchg;

    for (int i = 0; i < NUM_BUFFERS; i++) {
        channel->buffers[i] = mem_offset(base, offset);
        offset += channel->buffer_size;
    }

    return offset;
}


static void swap_channels(channel_t *channels[2])
{
    channel_t *tmp = channels[0];
    channels[0] = channels[1];
    channels[1] = tmp;
}


static int abx_mmap(abx_t *abx)
{
    abx->shm.base = mmap(NULL, abx->shm.size, PROT_READ | PROT_WRITE, MAP_SHARED, abx->shm.fd, 0);

    if (abx->shm.base == MAP_FAILED)
        return -errno;

    void *xchng[] = { abx->shm.base, mem_offset(abx->shm.base, sizeof(xchg_t))};
    channel_t *channels[] = { &abx->rx.channel, &abx->tx.channel };

    if (abx->owner)
        swap_channels(channels);

    size_t offset = get_header_size(abx);

    for (int i = 0; i < 2; i++)
        offset = map_channel(channels[i], abx->shm.base, xchng[i], offset);

    if (abx->owner) {
        atomic_store_explicit(abx->rx.channel.xchg, BUFFER_NONE, memory_order_relaxed);
        atomic_store_explicit(abx->tx.channel.xchg, BUFFER_NONE, memory_order_relaxed);
    }

    return 0;
}


static abx_t* abx_new(int fd, size_t rx_buffer_size, size_t tx_buffer_size)
{
    abx_t *abx = calloc(1, sizeof(abx_t));

    if (!abx)
        return abx;

    abx->alignment = get_alignment();

    abx->rx.channel.buffer_size = mem_align(rx_buffer_size, abx->alignment);
    abx->tx.channel.buffer_size = mem_align(tx_buffer_size, abx->alignment);

    abx->shm.size = get_header_size(abx);
    abx->shm.size += NUM_BUFFERS * abx->rx.channel.buffer_size;
    abx->shm.size += NUM_BUFFERS * abx->tx.channel.buffer_size;

    if (fd < 0) {
        abx->owner = true;
        abx->shm.fd = create_shm(abx->shm.size);
        if (abx->shm.fd < 0)
            goto create_shm_failed;
    } else {
        abx->owner = false;
        abx->shm.fd = fd;
    }

    int r = abx_mmap(abx);

    if (r < 0)
        goto mmap_failed;

    return abx;

mmap_failed:
    close(abx->shm.fd);
create_shm_failed:
    free(abx);
    return NULL;
}


abx_t* abx_owner_new(size_t rx_buffer_size, size_t tx_buffer_size)
{
    return abx_new(-1, rx_buffer_size, tx_buffer_size);
}


abx_t *abx_remote_new(int fd, size_t rx_buffer_size, size_t tx_buffer_size)
{
    return abx_new(fd, rx_buffer_size, tx_buffer_size);
}


void abx_delete(abx_t *abx)
{
    munmap(abx->shm.base, abx->shm.size);
    close(abx->shm.fd);
    free(abx);
}


size_t abx_size_rx(const abx_t *abx)
{
    return abx->rx.channel.buffer_size;
}


size_t abx_size_tx(const abx_t *abx)
{
    return abx->tx.channel.buffer_size;
}


int abx_get_fd(const abx_t *abx)
{
    return abx->shm.fd;
}


bool abx_ackd(const abx_t *abx)
{
    if (abx->tx.channel.buffer_size == 0)
        return false;

    unsigned xchg = atomic_load_explicit(abx->tx.channel.xchg, memory_order_relaxed);

    return xchg & LOCK_FLAG;
}


void* abx_recv(abx_t *abx)
{
    channel_t *channel = &abx->rx.channel;

    if (channel->buffer_size == 0)
        return  NULL;

    unsigned old = atomic_fetch_or_explicit(channel->xchg, LOCK_FLAG, memory_order_consume);

    buffer_idx_t current = (buffer_idx_t)(old & 0x3);

    if (current == BUFFER_NONE)
        return NULL;

    return channel->buffers[current];
}


void* abx_send(abx_t *abx)
{
    channel_t *channel = &abx->tx.channel;
    buffer_idx_t *current = &abx->tx.current;
    buffer_idx_t *locked = &abx->tx.locked;

    if (channel->buffer_size == 0)
        return  NULL;

    unsigned old = atomic_exchange_explicit(channel->xchg, *current, memory_order_release);

    if (old & LOCK_FLAG)
        *locked = (buffer_idx_t)(old & 0x3);

    *current = (*current + 1) % 3;

    if (*current == *locked)
        *current = (*current + 1) % 3;

    return channel->buffers[*current];
}



