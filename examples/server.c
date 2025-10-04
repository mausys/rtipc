#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


#include "rtipc.h"
#include "messages.h"

#define MAX_CYCLES 10000

typedef struct app {
    ri_consumer_t *command;
    ri_producer_t *response;
    ri_producer_t *event;
} app_t;



static void app_delete(app_t* app)
{
  if (app->command)
    ri_consumer_delete(app->command);
  if (app->response)
    ri_producer_delete(app->response);
  if (app->event)
    ri_producer_delete(app->event);
  free(app);
}

static void app_print_info(const app_t* app)
{
  ri_info_t info = ri_consumer_info(app->command);
  printf("command name = %s\n", (const char*)info.data);

  info = ri_producer_info(app->response);
  printf("response name = %s\n", (const char*)info.data);

  info = ri_producer_info(app->event);
  printf("event name = %s\n", (const char*)info.data);

}

static app_t* app_new(const char *path)
{

  ri_server_t *server = ri_server_new(path, 1);
  if (!server)
    goto fail_server;

  ri_vector_t *vec = ri_server_accept(server);

  ri_server_delete(server);
  if (!vec)
    goto fail_server;

  app_t *app = calloc(1, sizeof(app_t));

  if (!app)
    goto fail_alloc;


  app->command = ri_vector_take_consumer(vec, 0);
  if (!app->command)
    goto fail_channel;

  app->response = ri_vector_take_producer(vec, 0);
  if (!app->response)
    goto fail_channel;

  app->event = ri_vector_take_producer(vec, 1);
  if (!app->event)
    goto fail_channel;

  ri_vector_delete(vec);

  app_print_info(app);

  return app;

fail_channel:
  app_delete(app);
fail_alloc:
   ri_vector_delete(vec);
fail_server:
  return NULL;
}



static int32_t server_send_events(ri_producer_t *producer, uint32_t id, unsigned num, bool force)
{
  for (unsigned i = 0; i < num; i++) {
    printf("server send events:%u %u %b\n", id, i, force);
    msg_event_t *event = ri_producer_msg(producer);
    event->id = id;
    event->nr = i;
    if (force) {
      ri_producer_force_push(producer);
    } else {
      if (ri_producer_try_push(producer) == RI_PRODUCE_RESULT_FAIL) {
        return i;
      }
    }
  }
  return num;
}


static int32_t server_div(int32_t a, int32_t b, int32_t *res)
{
  if (b == 0) {
    return -1;
  } else {
    *res = a / b;
    return 0;
  }
}

void app_run(app_t *app)
{

  for (int i = 0; i < MAX_CYCLES; i++) {
    usleep(10000);
    bool run = true;
    ri_consume_result_t r = ri_consumer_pop(app->command);

    if ((r == RI_CONSUME_RESULT_NO_MSG) || (r == RI_CONSUME_RESULT_NO_UPDATE))
      continue;

    const msg_command_t *cmd = ri_consumer_msg(app->command);
    printf("server received:\n");
    msg_command_print(cmd);

    msg_response_t *rsp = ri_producer_msg(app->response);

    rsp->id = cmd->id;
    switch (cmd->id) {
    case CMDID_HELLO:
      rsp->result = 0;
      break;
    case CMDID_STOP:
      run = false;
      rsp->result = 0;
      break;
    case CMDID_SENDEVENT:
      rsp->result = server_send_events(app->event, cmd->args[0], cmd->args[1], cmd->args[2]);
      break;
    case CMDID_DIV:
      rsp->result = server_div(cmd->args[0], cmd->args[1], &rsp->data);
      break;
    default:
      rsp->result = -1;
      break;
    }
    ri_producer_force_push(app->response);
    if (!run)
      break;
  }
}



int main() {
  app_t* app = app_new("rtipc.sock");

  if (!app) {
    return -1;
  }

  app_run(app);

  printf("deleting server\n");
  app_delete(app);

  return 0;
}
