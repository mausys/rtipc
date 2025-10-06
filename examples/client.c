#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdatomic.h>
#include <threads.h>
#include <poll.h>

#include "rtipc.h"
#include "messages.h"


const ri_channel_param_t client2server_channels[] = {
    (ri_channel_param_t) { .add_msgs = 0, .msg_size = sizeof(msg_command_t), .eventfd = 1, .info = { .data = COMMAND_INFO, .size = sizeof(COMMAND_INFO) }},
  { 0 },
};


const ri_channel_param_t server2client_channels[] = {
  (ri_channel_param_t) { .add_msgs = 0, .msg_size = sizeof(msg_response_t), .eventfd = 1, .info = { .data = RESPONSE_INFO, .size = sizeof(RESPONSE_INFO) }},
  (ri_channel_param_t) { .add_msgs = 10, .msg_size = sizeof(msg_event_t), .eventfd = 1, .info = { .data = EVENT_INFO, .size = sizeof(EVENT_INFO) }},
  { 0 },
};



typedef struct app {
    ri_producer_t *command;
    ri_consumer_t *response;
    ri_consumer_t *event;
    thrd_t listener;
    atomic_bool run;
} app_t;




static msg_command_t commands[] = {
  (msg_command_t) {
      .id = CMDID_HELLO,
      .args = {1, 2, 0},
  },
  (msg_command_t) {
      .id = CMDID_SENDEVENT,
      .args = {11, 20, 0},
  },
  (msg_command_t) {
      .id = CMDID_SENDEVENT,
      .args = {12, 20, 1},
  },
  (msg_command_t) {
      .id = CMDID_DIV,
      .args = {100, 7, 0},
  },
  (msg_command_t) {
      .id = CMDID_DIV,
      .args = {100, 0, 0},
  },
  (msg_command_t) {
      .id = CMDID_STOP,
      .args = {0, 0, 0},
  },
  (msg_command_t) {
      .id = CMDID_UNKNOWN,
  },
};

static void app_delete(app_t *app)
{
  if (app->command)
    ri_producer_delete(app->command);
  if (app->response)
    ri_consumer_delete(app->response);
  if (app->event)
    ri_consumer_delete(app->event);
  free(app);
}

int event_listen(void *arg)
{
  app_t *app = arg;

  int eventfd = ri_consumer_eventfd(app->event);

  if (eventfd < 0)
    return eventfd;

  struct pollfd pollfd = {.fd = eventfd, .events = POLLIN, };

  while (atomic_load(&app->run)) {
    int r = poll(&pollfd, 1, 10);

    if (r < 0) {
      return -errno;
    }

    if (pollfd.revents & POLLIN) {
      ri_consume_result_t r = ri_consumer_pop(app->event);

      if ((r == RI_CONSUME_RESULT_NO_MSG) || (r == RI_CONSUME_RESULT_NO_UPDATE)) {
         printf("message queue empty");
      }

      msg_event_print(ri_consumer_msg(app->event));
    }

  }

  return 0;
}

static app_t* app_new(const char *path, const ri_channel_param_t *producers, const ri_channel_param_t *consumers, ri_info_t *info)
{


  ri_vector_t *vec =  ri_client_connect(path, producers, consumers, info);

  if (!vec)
    goto fail_connect;

  app_t *app = calloc(1, sizeof(app_t));

  if (!app)
    goto fail_alloc;

  app->command = ri_vector_take_producer(vec, 0);
  if (!app->command)
    goto fail_channel;

  app->response = ri_vector_take_consumer(vec, 0);
  if (!app->response)
    goto fail_channel;

  app->event = ri_vector_take_consumer(vec, 1);
  if (!app->event)
    goto fail_channel;

  atomic_store(&app->run, true);

  int r = thrd_create(&app->listener, event_listen, app);

  if (r != thrd_success) {
    goto fail_thread;
  }


  ri_vector_delete(vec);

  return app;

fail_thread:
fail_channel:
  app_delete(app);
fail_alloc:
  ri_vector_delete(vec);
fail_connect:
  return NULL;
}



void app_run(app_t *app, const msg_command_t *cmds)
{
  const msg_command_t *cmd = cmds;

  for (;;) {
    if (cmd->id == CMDID_UNKNOWN)
      return;

    *(msg_command_t*)ri_producer_msg(app->command) = *cmd;
    ri_producer_force_push(app->command);

    ri_consume_result_t r = ri_consumer_pop(app->response);

    if ((r == RI_CONSUME_RESULT_NO_MSG) || (r == RI_CONSUME_RESULT_NO_UPDATE)) {
      usleep(1000);
      continue;
    }

    printf("client received:\n");
    msg_response_print(ri_consumer_msg(app->response));
    cmd++;
  }

  usleep(10000);

  atomic_store(&app->run, false);

  thrd_join(app->listener, NULL);
}



int main() {

  app_t *app = app_new("rtipc.sock", client2server_channels, server2client_channels, NULL);

  if (!app) {
    return -1;
  }

  app_run(app, commands);

  printf("deleting client\n");
  app_delete(app);

  return 0;
}
