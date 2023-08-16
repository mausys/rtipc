#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct abx  abx_t;

abx_t* abx_owner_new(size_t rx_frame_size, size_t tx_frame_size);

abx_t *abx_remote_new(int fd, size_t rx_frame_size, size_t tx_frame_size);

void abx_delete(abx_t *abx);

size_t abx_size_rx(const abx_t *abx);

size_t abx_size_tx(const abx_t *abx);

int abx_get_fd(const abx_t *abx);

void* abx_recv(abx_t *abx);

void* abx_send(abx_t *abx);

bool abx_ackd(const abx_t *abx);

#ifdef __cplusplus
}
#endif
