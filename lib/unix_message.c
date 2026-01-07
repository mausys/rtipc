#include "unix_message.h"

#include <errno.h>
#include <stdint.h>
#include <stdalign.h>
#include <unistd.h>
#include <string.h>


#include <sys/types.h>
#include <sys/socket.h>
#include "mem_utils.h"

/* from kernel/include/net/scm.h */
#define SCM_MAX_FD     253



struct ri_uxmsg {
  void *data;
  size_t size;
  unsigned n_fds;
  alignas(struct cmsghdr) uint8_t cmsg[CMSG_SPACE(SCM_MAX_FD * sizeof(int))];
};


ri_uxmsg_t* ri_uxmsg_new(size_t size)
{
  ri_uxmsg_t *msg = malloc(sizeof(ri_uxmsg_t));

  if (!msg)
    goto fail_alloc;

  *msg = (ri_uxmsg_t) {
    .size = size,
  };

  msg->data = malloc(size);

  if (!msg->data)
    goto fail_msg;

  return msg;

fail_msg:
  free(msg);
fail_alloc:
  return NULL;
}


void ri_uxmsg_delete(ri_uxmsg_t *msg, bool close_fds)
{
  free(msg->data);

  struct cmsghdr *cmsghdr = (struct cmsghdr*) msg->cmsg;

  if (close_fds) {
    int *fds = (int*)CMSG_DATA(cmsghdr);

    for (unsigned i = 0; i < msg->n_fds; i++) {
      if (fds[i] > 0) {
        close(fds[i]);
      }
    }
  }

  free(msg);
}


void* ri_uxmsg_data(const ri_uxmsg_t *msg, size_t *size)
{
  if (size)
    *size = msg->size;

  return msg->data;
}


int ri_uxmsg_take_fd(ri_uxmsg_t *msg, unsigned index)
{
  if (index >= msg->n_fds)
    return -1;

  struct cmsghdr *cmsghdr = (struct cmsghdr*) msg->cmsg;

  int *fds = (int*)CMSG_DATA(cmsghdr);

  int fd = fds[index];

  fds[index] = -1;

  return fd;
}


int ri_uxmsg_add_fd(ri_uxmsg_t *msg, int fd)
{
  if (msg->n_fds >= SCM_MAX_FD)
    return -1;

  struct cmsghdr *cmsghdr = (struct cmsghdr*) msg->cmsg;

  int *fds = (int*)CMSG_DATA(cmsghdr);

  fds[msg->n_fds] = fd;

  msg->n_fds++;

  return 0;
}


int ri_uxmsg_send(const ri_uxmsg_t *msg, int socket)
{
  struct iovec iov = {
    .iov_base = msg->data,
    .iov_len = msg->size,
  };

  struct msghdr msghdr = {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = msg->n_fds > 0 ? (void*)msg->cmsg : NULL,
      .msg_controllen = msg->n_fds > 0 ? CMSG_SPACE(msg->n_fds * sizeof(int)) : 0,
  };

  if (msghdr.msg_controllen > 0) {
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msghdr);

    /* can't happen because n_fds is at least 1 (memfd) */
    if (!cmsg)
      return -1;

    *cmsg = (struct cmsghdr) {
        .cmsg_level = SOL_SOCKET,
        .cmsg_type = SCM_RIGHTS,
        .cmsg_len = CMSG_LEN(msg->n_fds * sizeof(int)),
    };
  }

  return sendmsg(socket, &msghdr, 0);
}


ri_uxmsg_t* ri_uxmsg_receive(int socket)
{
  struct msghdr msghdr = {
      .msg_iov = NULL,
      .msg_iovlen = 0,
      .msg_control = NULL,
      .msg_controllen = 0,
  };

  int r = recvmsg(socket, &msghdr, MSG_PEEK | MSG_TRUNC);

  if (r <= 0)
    goto fail_peek;

  ri_uxmsg_t* msg = ri_uxmsg_new(r);

  if (!msg)
    goto fail_peek;

  struct iovec iov = {
      .iov_base = msg->data,
      .iov_len = msg->size,
  };

  msghdr = (struct msghdr) {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = msg->cmsg,
      .msg_controllen = sizeof(msg->cmsg),
  };

  r = recvmsg(socket, &msghdr, 0);

  if (r != (int)msg->size)
    goto fail_recv;


  size_t cmsghdr_size = CMSG_ALIGN (sizeof (struct cmsghdr));

  if (msghdr.msg_controllen > cmsghdr_size) {
    const struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msghdr);
    if ((cmsg->cmsg_len  > cmsghdr_size)
     && (cmsg->cmsg_level == SOL_SOCKET)
     && (cmsg->cmsg_type == SCM_RIGHTS)) {
      size_t fds_size = cmsg->cmsg_len - cmsghdr_size;
      msg->n_fds = fds_size / sizeof(int);
    }
  }

  return msg;

fail_recv:
  ri_uxmsg_delete(msg, false);
fail_peek:
  return NULL;
}


int ri_uxsocket_send(int socket, const void *data, size_t size)
{
  struct iovec iov = {
      .iov_base = (void*)data,
      .iov_len = size,
  };

  struct msghdr msghdr = {
      .msg_iov = &iov,
      .msg_iovlen = 1,
  };

  return sendmsg(socket, &msghdr, 0);
}


void* ri_uxsocket_receive(int socket, size_t *size)
{
  if (!size)
    return NULL;

  struct msghdr msghdr = {
      .msg_iov = NULL,
      .msg_iovlen = 0,
      .msg_control = NULL,
      .msg_controllen = 0,
  };

  int r = recvmsg(socket, &msghdr, MSG_PEEK | MSG_TRUNC);

  if (r <= 0)
    goto fail_peek;

  *size = r;

  void* msg = calloc(*size, 1);

  if (!msg)
    goto fail_peek;

  struct iovec iov = {
      .iov_base = msg,
      .iov_len = *size,
  };

  msghdr = (struct msghdr) {
      .msg_iov = &iov,
      .msg_iovlen = 1,
  };

  r = recvmsg(socket, &msghdr, 0);

  if (r != (int)*size)
    goto fail_recv;

  return msg;

fail_recv:
  free(msg);
fail_peek:
  return NULL;
}



