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
  void *msg;
  size_t size;
  unsigned n_fds;
  unsigned index;
  alignas(struct cmsghdr) uint8_t cmsg[CMSG_SPACE(SCM_MAX_FD * sizeof(int))];
};


static int* get_fds(ri_uxmsg_t *req)
{
  struct cmsghdr *cmsghdr = (struct cmsghdr*) req->cmsg;

  return (int*)CMSG_DATA(cmsghdr);
}


ri_uxmsg_t* ri_uxmsg_new(size_t size)
{
  ri_uxmsg_t *req = malloc(sizeof(ri_uxmsg_t));
  if (!req)
    goto fail_alloc;

  *req = (ri_uxmsg_t) {
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


void ri_uxmsg_delete(ri_uxmsg_t *req, bool close_fds)
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


size_t ri_uxmsg_size(const ri_uxmsg_t *req)
{
  return req->size;
}


void* ri_uxmsg_ptr(const ri_uxmsg_t *req, size_t offset, size_t size)
{
  if (offset + size > req->size)
    return NULL;

  return mem_offset(req->msg, offset);
}


int ri_uxmsg_write(const ri_uxmsg_t *req, size_t *offset, const void *src, size_t size)
{
  void *ptr = ri_uxmsg_ptr(req, *offset, size);

  if (!ptr)
    return -1;

  memcpy(ptr, src, size);

  *offset += size;

  return 0;
}


int ri_uxmsg_read(const ri_uxmsg_t *req, size_t *offset, void *dst, size_t size)
{
  const void *ptr = ri_uxmsg_ptr(req, *offset, size);

  if (!ptr)
    return -1;

  memcpy(dst, ptr, size);

  *offset += size;

  return 0;
}

int ri_uxmsg_push_fd(ri_uxmsg_t *req, int fd)
{
  if (req->n_fds >= SCM_MAX_FD - 1)
    return -1;

  int *fds = get_fds(req);

  fds[req->n_fds] = fd;

  req->n_fds++;

  return 0;
}


int ri_uxmsg_pop_fd(ri_uxmsg_t *req)
{
  if (req->index >= req->n_fds)
    return -1;

  int *fds = get_fds(req);

  int fd = fds[req->index];

  fds[req->index] = -1;

  req->index++;

  return fd;
}


int ri_uxmsg_send(const ri_uxmsg_t *req, int socket)
{
  struct iovec iov = {
    .iov_base = req->msg,
    .iov_len = req->size,
  };

  struct msghdr msghdr = {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = req->n_fds > 0 ? (void*)req->cmsg : NULL,
      .msg_controllen = req->n_fds > 0 ? CMSG_SPACE(req->n_fds * sizeof(int)) : 0,
  };

  if (msghdr.msg_controllen > 0) {
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msghdr);

    /* can't happen because n_fds is at least 1 (memfd) */
    if (!cmsg)
      return -1;

    *cmsg = (struct cmsghdr) {
        .cmsg_level = SOL_SOCKET,
        .cmsg_type = SCM_RIGHTS,
        .cmsg_len = CMSG_LEN(req->n_fds * sizeof(int)),
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

  ri_uxmsg_t* req = ri_uxmsg_new(r);

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
  ri_uxmsg_delete(req, false);
fail_peek:
  return NULL;
}



