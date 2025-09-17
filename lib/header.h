#pragma once

#include <stdlib.h>

size_t ri_request_header_size(void);
int ri_request_header_validate(const void *request);
void ri_request_header_write(void *request);
