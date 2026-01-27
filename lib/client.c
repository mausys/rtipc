#include "rtipc.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "log.h"
#include "unix.h"

static int connect_path(const char *path)
{
  int r;
  int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);

  if (sockfd < 0) {
    r = -errno;
    LOG_ERR("socket failed errno=%u", errno);
    goto fail_socket;
  }

  struct sockaddr_un addr;

  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

  r = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));

  if (r < 0) {
    r = -errno;
    LOG_ERR("connect failed errno=%u", errno);
    goto fail_connect;
  }

  return sockfd;

fail_connect:
  close(sockfd);
fail_socket:
  return r;
}


static int exchange(int socket, ri_uxmsg_t *req)
{
  int r = ri_uxmsg_send(req, socket);

  if (r < 0) {
    LOG_ERR("ri_request_send failed r=%d", r);
    goto fail_send;
  }

  size_t response_size;
  void *response = ri_uxsocket_receive(socket, &response_size);

  if (!response) {
    r = -1;
    LOG_ERR("ri_uxsocket_receive failed");
    goto fail_receive;
  }

  int32_t result;

  if (response_size != sizeof(result)) {
    LOG_ERR("ri_uxsocket_receive failed");
    goto fail_response;

  }

  memcpy(&result, response, sizeof(result));

  free(response);

  return result;

fail_response:
  free(response);
fail_receive:
fail_send:
  return r;
}


static ri_uxmsg_t* uxmsg_from_resource(const ri_resource_t *rsc)
{
  size_t req_size = ri_request_calc_size(rsc);

  ri_uxmsg_t *req = ri_uxmsg_new(req_size);

  if (!req)
    goto fail_alloc;

  void *req_data = ri_uxmsg_data(req, &req_size);

  int r = ri_request_write(rsc, req_data, req_size);

  if (r < 0)
    goto fail_construct;

  r = ri_uxmsg_add_fd(req, rsc->shmfd);

  if (r < 0)
    goto fail_construct;

  for (const ri_channel_t *channel = rsc->producers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      r = ri_uxmsg_add_fd(req, channel->eventfd);

      if (r < 0)
        goto fail_construct;
    }
  }

  for (const ri_channel_t *channel = rsc->consumers; channel->msg_size != 0; channel++) {
    if (channel->eventfd > 0) {
      r = ri_uxmsg_add_fd(req, channel->eventfd);

      if (r < 0)
        goto fail_construct;
    }
  }

  return req;

fail_construct:
  ri_uxmsg_delete(req, false);
fail_alloc:
  return NULL;
}


ri_vector_t* ri_client_socket_connect(int socket, const ri_config_t *config)
{
  ri_resource_t *rsc = ri_resource_alloc(config);

  if (!rsc) {
    LOG_ERR("ri_transfer_new failed");
    goto fail_rsc;
  }

  ri_uxmsg_t *req = uxmsg_from_resource(rsc);

  if (!req) {
    LOG_ERR("uxmsg_from_resource failed");
    goto fail_req;
  }

  int r = exchange(socket, req);

  if (r < 0) {
    LOG_ERR("exchange failed");
    goto fail_exchange;
  }

  ri_vector_t *vec = ri_vector_new(rsc, false);

  if (!vec)
    goto fail_vec;

  ri_uxmsg_delete(req, false);
  ri_resource_delete(rsc);

  return vec;

fail_vec:
fail_exchange:
  ri_uxmsg_delete(req, false);
fail_req:
  ri_resource_delete(rsc);
fail_rsc:
  return NULL;
}


ri_vector_t* ri_client_connect(const char *path, const ri_config_t *config)
{
  int socket = connect_path(path);

  if (socket < 0) {
    return NULL;
  }

  ri_vector_t *vec = ri_client_socket_connect(socket, config);

  close(socket);

  return vec;
}
