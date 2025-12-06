#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef struct ri_uxmsg ri_uxmsg_t;

ri_uxmsg_t* ri_uxmsg_new(size_t size);

void ri_uxmsg_delete(ri_uxmsg_t *req, bool close_fds);

size_t ri_uxmsg_size(const ri_uxmsg_t *req);

void* ri_uxmsg_ptr(const ri_uxmsg_t *req, size_t offset, size_t size);

int ri_uxmsg_write(const ri_uxmsg_t *req, size_t *offset, const void *src, size_t size);

int ri_uxmsg_read(const ri_uxmsg_t *req, size_t *offset, void *dst, size_t size);

int ri_uxmsg_pop_fd(ri_uxmsg_t *req);

int ri_uxmsg_push_fd(ri_uxmsg_t *req, int fd);

int ri_uxmsg_send(const ri_uxmsg_t *req, int socket);
ri_uxmsg_t* ri_uxmsg_receive(int socket);
