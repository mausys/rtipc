#include "rtipc.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "log.h"
#include "unix_message.h"
#include "protocol.h"




static int client_transmit_request(const char *path, ri_uxmsg_t *req)
{
  int r = -1;
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

  r = ri_uxmsg_send(req, sockfd);

  if (r < 0) {
    LOG_ERR("ri_request_send failed r=%d", r);
    goto fail_send;
  }

  size_t response_size;
  void *response = ri_uxsocket_receive(sockfd, &response_size);

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
  close(sockfd);

  return result;

fail_response:
  free(response);
fail_receive:
fail_send:
fail_connect:
  close(sockfd);
fail_socket:
  return r;
}


ri_vector_t* ri_client_connect(const char *path, const ri_vector_param_t *vparam)
{
  ri_vector_t *vec = ri_vector_new(vparam);

  if (!vec) {
    LOG_ERR("ri_vector_new failed");
    goto fail_vec;
  }

  size_t req_size = ri_request_calc_size(vparam);

  ri_uxmsg_t *req = ri_uxmsg_new(req_size);

  if (!req)
    goto fail_req_alloc;

  void *req_data = ri_uxmsg_data(req, &req_size);

  int r = ri_request_write(vparam, req_data, req_size);

  if (r < 0)
    goto fail_req_init;

  r = ri_uxmsg_add_fd(req, ri_vector_get_shmfd(vec));

  if (r < 0)
    goto fail_req_init;


  for (unsigned i =  0; i < ri_vector_num_producers(vec); i++) {
    const ri_producer_t *producer = ri_vector_get_producer(vec, i);
    if (!producer)
      goto fail_req_init;

    int fd = ri_producer_eventfd(producer);

    if (fd > 0) {
      r = ri_uxmsg_add_fd(req, fd);

      if (r < 0)
        goto fail_req_init;
    }
  }

  for (unsigned i =  0; i < ri_vector_num_consumers(vec); i++) {
    const ri_consumer_t *consumer = ri_vector_get_consumer(vec, i);
    if (!consumer)
      goto fail_req_init;

    int fd = ri_consumer_eventfd(consumer);

    if (fd > 0) {
      r = ri_uxmsg_add_fd(req, fd);

      if (r < 0)
        goto fail_req_init;
    }
  }

  r = client_transmit_request(path, req);

  if (r < 0) {
    LOG_ERR("client_send_vector failed");
    goto fail_transmit;
  }

  ri_uxmsg_delete(req, false);

  return vec;

fail_transmit:
fail_req_init:
  ri_uxmsg_delete(req, false);
fail_req_alloc:
  ri_vector_delete(vec);
fail_vec:
  return NULL;
}
