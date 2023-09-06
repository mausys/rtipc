#include "rtipc/abx.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>


#include "shm.h"
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


struct abx {
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


static size_t get_header_size(void)
{
    return mem_align(2 * sizeof(xchg_t), cache_line_size());
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


static void abx_map(abx_t *abx, bool owner)
{
    void *xchng[] = { abx->shm.base, mem_offset(abx->shm.base, sizeof(xchg_t)) };
    channel_t *channels[] = { &abx->rx.channel, &abx->tx.channel };

    if (owner)
        swap_channels(channels);

    size_t offset = get_header_size();

    for (int i = 0; i < 2; i++)
        offset = map_channel(channels[i], abx->shm.base, xchng[i], offset);
}


static abx_t* abx_new(int fd, size_t rx_buffer_size, size_t tx_buffer_size)
{
    int r;
    size_t size;
    abx_t *abx = calloc(1, sizeof(abx_t));
    bool owner = fd < 0;

    if (!abx)
        return abx;

    abx->rx.channel.buffer_size = mem_align(rx_buffer_size, cache_line_size());
    abx->tx.channel.buffer_size = mem_align(tx_buffer_size, cache_line_size());

    size = get_header_size();
    size += NUM_BUFFERS * abx->rx.channel.buffer_size;
    size += NUM_BUFFERS * abx->tx.channel.buffer_size;

    if (owner)
        r = shm_create(&abx->shm, size);
    else
        r = shm_map(&abx->shm, size, fd);

    if (r < 0)
        goto fail;

    abx_map(abx, owner);

    if (owner) {
        atomic_store_explicit(abx->rx.channel.xchg, BUFFER_NONE, memory_order_relaxed);
        atomic_store_explicit(abx->tx.channel.xchg, BUFFER_NONE, memory_order_relaxed);
    }

    return abx;
fail:
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
    shm_destroy(&abx->shm);
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



