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
#include "request.h"
#include "protocol.h"

typedef struct ri_server ri_server_t;

struct ri_server {
  int sockfd;
  struct sockaddr_un addr;
};

int ri_socket_pair(int sockets[2])
{
  return socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets);
}


ri_server_t* ri_server_new(const char* path)
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

  r = listen(server->sockfd, 1);

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


ri_vector_t* ri_server_accept(const ri_server_t* server)
{
  struct sockaddr_un addr;
  socklen_t socklen = sizeof(addr);
  int cfd = accept(server->sockfd, (struct sockaddr*)&addr, &socklen);

  if (cfd < 0) {
    LOG_ERR("accept failed errno=%u", errno);
    goto fail;
  }

  ri_request_t *req = ri_request_receive(cfd);

  close(cfd);

  if (!req)
    goto fail;

  ri_vector_t *vec = ri_vector_from_request(req);

  ri_request_delete(req, true);

  if (!vec) {
    goto fail;
  }

  return vec;

fail:
  return NULL;
}

void ri_server_delete(ri_server_t* server)
{
  close(server->sockfd);
  unlink(server->addr.sun_path);
  free(server);
}
