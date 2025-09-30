#pragma once

int ri_eventfd(void);

int ri_check_memfd(int fd);

int ri_check_eventfd(int fd);

int ri_set_nonblocking(int fd);
