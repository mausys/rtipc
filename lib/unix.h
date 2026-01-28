#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef struct ri_uxmsg ri_uxmsg_t;


int ri_shmfd_create(size_t size);


int ri_eventfd(void);


int ri_check_memfd(int fd);


int ri_check_eventfd(int fd);


int ri_set_nonblocking(int fd);


ri_uxmsg_t* ri_uxmsg_new(size_t size);


void ri_uxmsg_delete(ri_uxmsg_t *msg, bool close_fds);


void* ri_uxmsg_data(const ri_uxmsg_t *msg, size_t *size);


int ri_uxmsg_take_fd(ri_uxmsg_t *msg, unsigned index);


int ri_uxmsg_add_fd(ri_uxmsg_t *msg, int fd);


int ri_uxmsg_send(const ri_uxmsg_t *msg, int socket);

ri_uxmsg_t* ri_uxmsg_receive(int socket);


int ri_uxsocket_send(int socket, const void *data, size_t size);

void* ri_uxsocket_receive(int socket, size_t *size);
