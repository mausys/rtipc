#include "fd.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


static const char c_memfd_link[] = "/memfd:";
static const char c_eventfd_link[] = "anon_inode:[eventfd";

int ri_fd_check(int fd, ri_fd_t expected)
{
  char path[32];
  char link[32];
  snprintf(path, sizeof(path), "/proc/self/fd/%d" , fd);
  ssize_t r = readlink(path, link, sizeof(link));

  if ((r < 0) || ((size_t)r >= sizeof(link) - 1))
    return -1;

  switch (expected) {
    case RI_FD_EVENT:
      r = strncmp(link, c_eventfd_link, sizeof(c_eventfd_link) - 1) == 0 ? 0 : -1;
      break;
    case RI_FD_MEM:
      r = strncmp(link, c_memfd_link, sizeof(c_memfd_link) - 1) == 0 ? 0 : -1;
      break;
  }

  return r;
}


int ri_fd_set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  return r >= 0 ? r : -errno;
}


