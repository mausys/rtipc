#include "messages.h"

#include <stdio.h>


void msg_command_print(const msg_command_t *msg)
{
  printf("command: \tid=0x%x\n", msg->id);
  printf("\targ[0]=0x%x\n", msg->args[0]);
  printf("\targ[1]=0x%x\n", msg->args[1]);
  printf("\targ[2]=0x%x\n", msg->args[2]);
}


void msg_response_print(const msg_response_t *msg)
{
  printf("response: \tid=0x%x\n", msg->id);
  printf("\tresult=%i\n", msg->result);
  printf("\tdata[1]=0x%x\n", msg->data);
}


void msg_event_print(const msg_event_t *msg)
{
  printf("event: \tid=%u\n", msg->id);
  printf("\tnr=%u\n", msg->nr);
}

