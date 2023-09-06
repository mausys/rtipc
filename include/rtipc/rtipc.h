#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtipc rtipc_t;

typedef struct rtipc_object {
    void *p;
    size_t size;
    size_t align;

} rtipc_object_t;

#define RTIPC_OBJECT_ITEM(x) { .p = &(x), .size = sizeof(*(x)), .align = __alignof__(*(x)) }
#define RTIPC_OBJECT_NULL(s) { .p = NULL, .size = (s) }

rtipc_t* rtipc_owner_new(const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs, bool cache);

rtipc_t* rtipc_remote_new(int fd, const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs, bool cache);

void rtipc_delete(rtipc_t *rtipc);

int rtipc_get_fd(const rtipc_t *rtipc);

int rtipc_send(rtipc_t *rtipc);

int rtipc_recv(rtipc_t *rtipc);

bool rtipc_ackd(const rtipc_t *rtipc);

#ifdef __cplusplus
}
#endif
