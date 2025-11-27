#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef struct ri_request ri_request_t;

ri_request_t* ri_request_new(size_t size);

void ri_request_delete(ri_request_t *req, bool close_fds);

size_t ri_request_size(const ri_request_t *req);

void* ri_request_ptr(const ri_request_t *req, size_t offset, size_t size);

int ri_request_write(const ri_request_t *req, size_t *offset, const void *src, size_t size);

int ri_request_read(const ri_request_t *req, size_t *offset, void *dst, size_t size);

int ri_request_pop_fd(ri_request_t *req);

int ri_request_push_fd(ri_request_t *req, int fd);

int ri_request_send(const ri_request_t *req, int socket);
ri_request_t* ri_request_receive(int socket);
