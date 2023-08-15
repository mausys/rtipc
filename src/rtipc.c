#include "rtipc.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "abx.h"
#include "mem_utils.h"


#define RTIPC_DATA_ALIGN sizeof(uint64_t)


typedef struct
{
    size_t offset;
    size_t size;
    void **p;
} object_map_t;


struct rtipc
{
    abx_t *abx;
    struct {
        void *shm;
        object_map_t *map;
        unsigned num;
        size_t size;
    } rx;
    struct {
        void *shm;
        void *cache;
        size_t size;
    } tx;
};


static void map_opjects(void *base, object_map_t map[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        if (map[i].p)
            *map[i].p = mem_offset(base, map[i].offset);
    }
}


static size_t init_objects(object_map_t map[], const rtipc_object_t *objects, unsigned n)
{
    size_t offset = 0;

    for (unsigned i = 0; i < n; i++) {
        map[i] = (object_map_t) {
            .p = objects[i].p,
            .size = objects[i].size,
            .offset = offset,
        };

        offset += map[i].size;
        offset = mem_align(offset, RTIPC_DATA_ALIGN);

        if (map[i].p)
            *map[i].p = NULL;
    }

    return offset;
}


static int init_rx(rtipc_t *rtipc, const rtipc_object_t *objects, unsigned n)
{
    if (n == 0)
        return 0;

    rtipc->rx.map = malloc(n * sizeof(object_map_t));

    if (!rtipc->rx.map)
        return -ENOMEM;

    rtipc->rx.size = init_objects(rtipc->rx.map, objects, n);
    rtipc->rx.num = n;

    return 0;
}


static int init_tx(rtipc_t *rtipc, const rtipc_object_t *objects, unsigned n)
{
    if (n == 0)
        return 0;

    object_map_t *map = malloc(n * sizeof(object_map_t));

    if (!map)
        return -ENOMEM;

    rtipc->tx.size = init_objects(map, objects, n);

    rtipc->tx.cache = calloc(1, rtipc->tx.size);

    if (!rtipc->tx.cache)
        goto fail_cache;

    map_opjects(rtipc->tx.cache, map, n);

    free(map);

    return 0;

fail_cache:
    free(map);
    rtipc->tx.size = 0;
    return -ENOMEM;
}


static rtipc_t* rtipc_new(int fd, const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs)
{
    int r;
    rtipc_t *rtipc = calloc(1, sizeof(rtipc_t));

    if (!rtipc)
        return rtipc;

    r = init_rx(rtipc, rx_objects, nrobjs);

    if (r < 0)
        goto fail;


    r = init_tx(rtipc, tx_objects, ntobjs);

    if (r < 0)
        goto fail;

    if (fd < 0)
        rtipc->abx = abx_owner_new(rtipc->rx.size, rtipc->tx.size);
    else
        rtipc->abx = abx_remote_new(fd, rtipc->rx.size, rtipc->tx.size);

    if (!rtipc->abx)
        goto fail;

    rtipc->tx.shm = abx_send(rtipc->abx);

    return rtipc;

fail:
    rtipc_delete(rtipc);
    return NULL;
}


rtipc_t* rtipc_server_new(const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs)
{
    return rtipc_new(-1, rx_objects, nrobjs, tx_objects, ntobjs);
}


rtipc_t* rtipc_client_new(int fd, const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs)
{
    return rtipc_new(fd, rx_objects, nrobjs, tx_objects, ntobjs);
}


void rtipc_delete(rtipc_t *rtipc)
{
    if (rtipc->rx.map) {
        free(rtipc->rx.map);
        rtipc->rx.map = NULL;
    }

    if (rtipc->tx.cache) {
        free(rtipc->tx.cache);
        rtipc->tx.cache = 0;
    }

    if (rtipc->abx) {
        abx_delete(rtipc->abx);
        rtipc->abx = NULL;
    }

    free(rtipc);
}


int rtipc_get_fd(rtipc_t *rtipc)
{
    return abx_get_fd(rtipc->abx);
}


int rtipc_send(rtipc_t *rtipc)
{
    int r = -1;

    if (rtipc->tx.shm) {
        memcpy(rtipc->tx.shm, rtipc->tx.cache, rtipc->tx.size);
        r = 0;
    }

    rtipc->tx.shm = abx_send(rtipc->abx);

    return r;
}


bool rtipc_ackd(rtipc_t *rtipc)
{
    return abx_ackd(rtipc->abx);
}


int rtipc_recv(rtipc_t *rtipc)
{
    void *old = rtipc->rx.shm;

    rtipc->rx.shm = abx_recv(rtipc->abx);

    if (!rtipc->rx.shm)
        return -1;

    if (old == rtipc->rx.shm)
        return 0;

    map_opjects(rtipc->rx.shm, rtipc->rx.map, rtipc->rx.num);

    return 1;
}


