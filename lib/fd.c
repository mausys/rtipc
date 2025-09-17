#include "fd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char c_memfd_link[] = "/memfd:";
static const char c_eventfd_link[] = "anon_inode:[eventfd";

ri_fd_t ri_fd_get_type(int fd)
{
  char path[32];
  char link[32];
  snprintf(path, sizeof(path), "/proc/self/fd/%d" , fd);
  ssize_t r = readlink(path, link, sizeof(link));

  if ((r < 0) || ((size_t)r >= sizeof(link) - 1))
    return RI_FD_UNKNOWN;

  /* make sure buf is null terminated */
  link[r] = 0;

  if (strncmp(link, c_eventfd_link, sizeof(c_eventfd_link) - 1) == 0)
    return RI_FD_EVENT;

  if (strncmp(link, c_memfd_link, sizeof(c_memfd_link) - 1) == 0)
    return RI_FD_MEM;

  return RI_FD_UNKNOWN;
}
