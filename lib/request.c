#include "request.h"

#include <errno.h>
#include <stdint.h>
#include <stdalign.h>
#include <unistd.h>
#include <string.h>


#include <sys/types.h>
#include <sys/socket.h>
#include "mem_utils.h"

#include "protocol.h"

/* from kernel/include/net/scm.h */
#define SCM_MAX_FD     253


struct ri_request {
  void *msg;
  size_t size;
  unsigned n_fds;
  unsigned index;
  alignas(struct cmsghdr) uint8_t cmsg[CMSG_SPACE(SCM_MAX_FD * sizeof(int))];
};


static int* get_fds(ri_request_t *req)
{
  struct cmsghdr *cmsghdr = (struct cmsghdr*) req->cmsg;

  return (int*)CMSG_DATA(cmsghdr);
}


ri_request_t* ri_request_new(size_t size)
{
  ri_request_t *req = malloc(sizeof(ri_request_t));
  if (!req)
    goto fail_alloc;

  *req = (ri_request_t) {
    .size = size,
  };

  req->msg = malloc(size);

  if (!req->msg)
    goto fail_msg;

  return req;

fail_msg:
  free(req);
fail_alloc:
  return NULL;
}


void ri_request_delete(ri_request_t *req, bool close_fds)
{
  free(req->msg);

  if (close_fds) {
    int *fds = get_fds(req);

    for (unsigned i = 0; i < req->n_fds; i++) {
      if (fds[i] > 0) {
        close(fds[i]);
      }
    }
  }
  free(req);
}


size_t ri_request_size(const ri_request_t *req)
{
  return req->size;
}


void* ri_request_ptr(const ri_request_t *req, size_t offset, size_t size)
{
  if (offset + size > req->size)
    return NULL;

  return mem_offset(req->msg, offset);
}


int ri_request_write(const ri_request_t *req, size_t *offset, const void *src, size_t size)
{
  void *ptr = ri_request_ptr(req, *offset, size);

  if (!ptr)
    return -1;

  memcpy(ptr, src, size);

  *offset += size;

  return 0;
}


int ri_request_read(const ri_request_t *req, size_t *offset, void *dst, size_t size)
{
  const void *ptr = ri_request_ptr(req, *offset, size);

  if (!ptr)
    return -1;

  memcpy(dst, ptr, size);

  *offset += size;

  return 0;
}

int ri_request_push_fd(ri_request_t *req, int fd)
{
  if (req->n_fds >= SCM_MAX_FD - 1)
    return -1;

  int *fds = get_fds(req);

  fds[req->n_fds] = fd;

  req->n_fds++;

  return 0;
}


int ri_request_pop_fd(ri_request_t *req)
{
  if (req->index >= req->n_fds)
    return -1;

  int *fds = get_fds(req);

  int fd = fds[req->index];

  fds[req->index] = -1;

  req->index++;

  return fd;
}


int ri_request_send(const ri_request_t *req, int socket)
{
  struct iovec iov = {
    .iov_base = req->msg,
    .iov_len = req->size,
  };

  struct msghdr msghdr = {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = (void*)req->cmsg,
      .msg_controllen = CMSG_SPACE(req->n_fds * sizeof(int)),
  };

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msghdr);

  /* can't happen because n_fds is at least 1 (memfd) */
  if (!cmsg)
    return -1;

  *cmsg = (struct cmsghdr) {
      .cmsg_level = SOL_SOCKET,
      .cmsg_type = SCM_RIGHTS,
      .cmsg_len = CMSG_LEN(req->n_fds * sizeof(int)),
  };

  return sendmsg(socket, &msghdr, 0);
}


ri_request_t* ri_request_receive(int socket)
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

  ri_request_t* req = ri_request_new(r);

  if (!req)
    goto fail_peek;

  struct iovec iov = {
      .iov_base = req->msg,
      .iov_len = req->size,
  };

  msghdr = (struct msghdr) {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = req->cmsg,
      .msg_controllen = sizeof(req->cmsg),
  };

  r = recvmsg(socket, &msghdr, 0);

  if (r != (int)req->size)
    goto fail_recv;


  size_t cmsghdr_size = CMSG_ALIGN (sizeof (struct cmsghdr));

  if (msghdr.msg_controllen > cmsghdr_size) {
    const struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msghdr);
    if ((cmsg->cmsg_len  > cmsghdr_size)
     && (cmsg->cmsg_level == SOL_SOCKET)
     && (cmsg->cmsg_type == SCM_RIGHTS)) {
      size_t fds_size = cmsg->cmsg_len - cmsghdr_size;
      req->n_fds = fds_size / sizeof(int);
    }
  }

  return req;

fail_recv:
  ri_request_delete(req, false);
fail_peek:
  return NULL;
}



