#include "event.h"

#include <sys/eventfd.h>


int ri_event_create(void)
{
  return eventfd(0, EFD_CLOEXEC);
}
