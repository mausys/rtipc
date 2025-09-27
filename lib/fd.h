#pragma once


typedef enum {
  RI_FD_EVENT,
  RI_FD_MEM,
} ri_fd_t;


int ri_fd_check(int fd, ri_fd_t expected);

int ri_fd_set_nonblocking(int fd);
