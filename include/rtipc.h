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
} rtipc_object_t;

#define RTIPC_OBJECT_ITEM(x) { .p = &(x), .size = sizeof(*(x)) }
#define RTIPC_OBJECT_NULL(s) { .p = NULL, .size = (s) }

rtipc_t* rtipc_server_new(const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs);

rtipc_t* rtipc_client_new(int fd, const rtipc_object_t *rx_objects, unsigned nrobjs, const rtipc_object_t *tx_objects, unsigned ntobjs);

void rtipc_delete(rtipc_t *rtipc);

int rtipc_get_fd(const rtipc_t *rtipc);

int rtipc_send(rtipc_t *rtipc);

int rtipc_recv(rtipc_t *rtipc);

bool rtipc_ackd(const rtipc_t *rtipc);

#ifdef __cplusplus
}
#endif
