#include "rtipc/rtipc.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "rtipc/abx.h"
#include "mem_utils.h"


typedef struct
{
    size_t offset;
    size_t size;
    void **p;
} object_map_t;

typedef struct {
    void *shm;
    object_map_t *map;
    unsigned num;
    size_t size;
} channel_t;

struct rtipc
{
    abx_t *abx;
    channel_t rx;
    channel_t tx;
    void *cache;
};


static void nullify_opjects(object_map_t map[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (map[i].p)
            *map[i].p = NULL;
    }
}


static void map_opjects(void *base, object_map_t map[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (map[i].p)
            *map[i].p = mem_offset(base, map[i].offset);
    }
}


static int init_channel(channel_t *channel, const rtipc_object_t *objects, unsigned n)
{
    if (n == 0)
        return 0;

    channel->map = malloc(n * sizeof(object_map_t));

    if (!channel->map)
        return -ENOMEM;

    channel->num = n;

    for (unsigned i = 0; i < channel->num; i++) {

        if (objects[i].align != 0)
            channel->size = mem_align(channel->size, objects[i].align);

        channel->map[i] = (object_map_t) {
            .p = objects[i].p,
            .size = objects[i].size,
            .offset = channel->size,
        };

        channel->size += channel->map[i].size;


        if (channel->map[i].p)
            *channel->map[i].p = NULL;
    }

    return 0;
}


static rtipc_t* rtipc_new(int fd, const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs, bool cache)
{
    int r;
    rtipc_t *rtipc = calloc(1, sizeof(rtipc_t));

    if (!rtipc)
        return NULL;

    r = init_channel(&rtipc->rx, rx_objects, nrobjs);

    if (r < 0)
        goto fail;

    r = init_channel(&rtipc->tx, tx_objects, ntobjs);

    if (r < 0)
        goto fail;

    if (fd < 0)
        rtipc->abx = abx_owner_new(rtipc->rx.size, rtipc->tx.size);
    else
        rtipc->abx = abx_remote_new(fd, rtipc->rx.size, rtipc->tx.size);

    if (!rtipc->abx)
        goto fail;

    rtipc->tx.shm = abx_send(rtipc->abx);

    if (cache) {
        rtipc->cache = calloc(1, rtipc->tx.size);

        if (!rtipc->cache)
            goto fail;

        map_opjects(rtipc->cache, rtipc->tx.map, rtipc->tx.num);
    } else {
        map_opjects(rtipc->tx.shm, rtipc->tx.map, rtipc->tx.num);
    }

    return rtipc;

fail:
    rtipc_delete(rtipc);
    return NULL;
}


rtipc_t* rtipc_server_new(const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs, bool cache)
{
    return rtipc_new(-1, rx_objects, nrobjs, tx_objects, ntobjs, cache);
}


rtipc_t* rtipc_client_new(int fd, const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs, bool cache)
{
    return rtipc_new(fd, rx_objects, nrobjs, tx_objects, ntobjs, cache);
}


void rtipc_delete(rtipc_t *rtipc)
{
    if (rtipc->rx.map) {
        nullify_opjects(rtipc->rx.map, rtipc->rx.num);
        free(rtipc->rx.map);
        rtipc->rx.map = NULL;
    }

    if (rtipc->tx.map) {
        nullify_opjects(rtipc->tx.map, rtipc->tx.num);
        free(rtipc->tx.map);
        rtipc->tx.map = NULL;
    }

    if (rtipc->cache) {
        free(rtipc->cache);
        rtipc->cache = 0;
    }

    if (rtipc->abx) {
        abx_delete(rtipc->abx);
        rtipc->abx = NULL;
    }

    free(rtipc);
}


int rtipc_get_fd(const rtipc_t *rtipc)
{
    return abx_get_fd(rtipc->abx);
}


int rtipc_send(rtipc_t *rtipc)
{
    if (!rtipc->tx.shm)
        return -1;

    if (rtipc->cache) {
        memcpy(rtipc->tx.shm, rtipc->cache, rtipc->tx.size);
        rtipc->tx.shm = abx_send(rtipc->abx);
    } else {
        rtipc->tx.shm = abx_send(rtipc->abx);
        map_opjects(rtipc->tx.shm, rtipc->tx.map, rtipc->tx.num);
    }

    return 0;
}


bool rtipc_ackd(const rtipc_t *rtipc)
{
    return abx_ackd(rtipc->abx);
}


int rtipc_recv(rtipc_t *rtipc)
{
    void *old = rtipc->rx.shm;

    rtipc->rx.shm = abx_recv(rtipc->abx);

    if (!rtipc->rx.shm) {
        nullify_opjects(rtipc->rx.map, rtipc->rx.num);
        return -1;
    }

    if (old == rtipc->rx.shm)
        return 0;

    map_opjects(rtipc->rx.shm, rtipc->rx.map, rtipc->rx.num);

    return 1;
}


