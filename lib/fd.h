#pragma once


typedef enum {
  RI_FD_UNKNOWN,
  RI_FD_EVENT,
  RI_FD_MEM,
} ri_fd_t;


ri_fd_t ri_fd_get_type(int fd);
