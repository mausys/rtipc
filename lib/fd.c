#include "fd.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/eventfd.h>

#include "log.h"


#define PROC_SELF_FORMAT "/proc/self/fd/%d"




int ri_eventfd(void)
{
  return eventfd(0, EFD_CLOEXEC | EFD_SEMAPHORE | EFD_NONBLOCK);
}



int ri_check_memfd(int fd)
{
  char path[32];
  char link[32];
  const char expected[] = "/memfd:";

  snprintf(path, sizeof(path), PROC_SELF_FORMAT, fd);

  ssize_t r = readlink(path, link, sizeof(link));

  if ((r < 0) || ((size_t)r < sizeof(expected)))
    return -1;

  return strncmp(link, expected, sizeof(expected) - 1) == 0 ? 0 : -1;
}


int ri_check_eventfd(int fd)
{
  char path[32];
  char link[32];
  const char expected[] = "anon_inode:[eventfd";

  snprintf(path, sizeof(path), PROC_SELF_FORMAT, fd);

  ssize_t r = readlink(path, link, sizeof(link));

  if ((r < 0) || ((size_t)r < sizeof(expected)))
    return -1;

  return strncmp(link, expected, sizeof(expected) - 1) == 0 ? 0 : -1;
}




int ri_set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  if (r < 0) {
    r = -errno;
    LOG_ERR("ri_set_nonblocking failed for %d errno-%d", fd , -r);
  }

  return r;
}


