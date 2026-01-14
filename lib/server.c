#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "rtipc.h"
#include "log.h"
#include "unix.h"

typedef struct ri_server ri_server_t;

struct ri_server {
  int sockfd;
  struct sockaddr_un addr;
};

int ri_socket_pair(int sockets[2])
{
  return socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets);
}


ri_server_t* ri_server_new(const char* path, int backlog)
{
  ri_server_t *server = malloc(sizeof(ri_server_t));

  if (!server) {
    goto fail_alloc;
  }

  server->sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);

  if (server->sockfd < 0) {
    LOG_ERR("socket failed errno=%u", errno);
    goto fail_socket;
  }

  server->addr.sun_family = AF_UNIX;
  snprintf(server->addr.sun_path, sizeof(server->addr.sun_path), "%s", path);

  int r = bind(server->sockfd, (struct sockaddr*)&server->addr, SUN_LEN(&server->addr));

  if (r < 0) {
    LOG_ERR("bind (%s) failed errno=%u", path, errno);
    goto fail_bind;
  }

  r = listen(server->sockfd, backlog);

  if (r < 0) {
    LOG_ERR("listen (%s) failed errno=%u", path, errno);
    goto fail_bind;
  }

  return server;

fail_bind:
  close(server->sockfd);
fail_socket:
  free(server);
fail_alloc:
  return NULL;
}


int ri_server_socket(const ri_server_t* server)
{
  return server->sockfd;
}


static int server_send_response(int socket, int32_t result)
{

  return ri_uxsocket_send(socket, &result, sizeof(result));
}


static ri_resource_t* request_to_resources(ri_uxmsg_t *req)
{
  size_t size;
  const void *data = ri_uxmsg_data(req, &size);
  ri_resource_t *rsc = ri_request_parse(data, size);

  if (!rsc) {
    LOG_ERR("ri_request_parse failed");
    goto fail_map;
  }

  unsigned fd_index = 0;

  rsc->shmfd = ri_uxmsg_take_fd(req, fd_index++);

  for (ri_channel_t *channel = rsc->consumers; channel->msg_size != 0; channel++) {
    if (channel->eventfd <= 0)
      continue;

    channel->eventfd = ri_uxmsg_take_fd(req, fd_index++);

    if (channel->eventfd <= 0)
      goto fail_eventfd;
  }

  for (ri_channel_t *channel = rsc->producers; channel->msg_size != 0; channel++) {
    if (channel->eventfd <= 0)
      continue;

    channel->eventfd = ri_uxmsg_take_fd(req, fd_index++);

    if (channel->eventfd <= 0)
      goto fail_eventfd;
  }

  return rsc;

fail_eventfd:
  ri_resource_delete(rsc);
fail_map:
  return NULL;
}


ri_vector_t* ri_server_accept(const ri_server_t* server, ri_filter_fn filter, void *user_data)
{
  int cfd = accept(server->sockfd, NULL, NULL);

  if (cfd < 0) {
    LOG_ERR("accept failed errno=%u", errno);
    goto fail_accept;
  }

  ri_uxmsg_t *req = ri_uxmsg_receive(cfd);

  if (!req) {
     LOG_ERR("ri_uxmsg_receive failed");
    goto fail_receive;
  }

  ri_resource_t *rsc = request_to_resources(req);

  if (!rsc)
    goto fail_transfer;

  if (filter) {
    if (!filter(rsc, user_data)) {
      LOG_INF("server rejected request");
      goto fail_rejected;
    }
  }

  ri_vector_t *vec = ri_vector_new(rsc, true);

  if (!vec) {
    goto fail_rejected;
  }

  ri_vector_init_shm(vec);

  server_send_response(cfd, 0);

  ri_uxmsg_delete(req, true);

  close(cfd);

  return vec;

fail_rejected:
  ri_resource_delete(rsc);
fail_transfer:
  ri_uxmsg_delete(req, true);
fail_receive:
  server_send_response(cfd, -1);
  close(cfd);
fail_accept:
  return NULL;
}

void ri_server_delete(ri_server_t* server)
{
  close(server->sockfd);
  unlink(server->addr.sun_path);
  free(server);
}
