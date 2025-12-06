#include "rtipc.h"

#include <errno.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "log.h"
#include "unix_message.h"
#include "protocol.h"




ri_vector_t* ri_client_connect(const char *path, const ri_channel_param_t producers[], const ri_channel_param_t consumers[], const ri_info_t *info)
{

  ri_vector_t *vec = ri_vector_new(producers, consumers, info);

  if (!vec) {
    LOG_ERR("ri_vector_new failed");
    goto fail_vec;
  }

  ri_uxmsg_t *req = ri_request_create(vec);

  if (!req) {
    LOG_ERR("ri_request_from_vector failed");
    goto fail_req;
  }

  int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);

  if (sockfd < 0) {
    LOG_ERR("socket failed errno=%u", errno);
    goto fail_socket;
  }

  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

  int r = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));

  if (r < 0) {
    LOG_ERR("connect failed errno=%u", errno);
    goto fail_socket;
  }

  r = ri_uxmsg_send(req, sockfd);

  if (r < 0) {
    LOG_ERR("ri_request_send failed r=%d", r);
    goto fail_send;
  }

  ri_uxmsg_delete(req, false);

  return vec;

fail_send:
fail_socket:
  ri_uxmsg_delete(req, false);
fail_req:
  ri_vector_delete(vec);
fail_vec:
  return NULL;
}
