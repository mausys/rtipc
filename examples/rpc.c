#include <stdlib.h>
#include <unistd.h>
#include <threads.h>
#include <stdio.h>
#include <stdatomic.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "../lib/rtipc.h"

#define MAX_CYCLES 10000

typedef enum  {
    CMDID_UNKNOWN = 0,
    CMDID_HELLO,
    CMDID_STOP,
    CMDID_SENDEVENT,
    CMDID_DIV,
} command_id_t;

typedef struct msg_command {
    command_id_t id;
    int32_t args[3];
} msg_command_t;


typedef struct msg_response {
    command_id_t id;
    int32_t result;
    int32_t data;
} msg_response_t;

typedef struct msg_event {
    uint32_t id;
    uint32_t nr;
} msg_event_t;


#define  COOKIE 0x13579bdf

const ri_channel_param_t client2server_channels[] = {
  (ri_channel_param_t) { .add_msgs = 0, .msg_size = sizeof(msg_command_t), },
  { 0 },
};


const ri_channel_param_t server2client_channels[] = {
  (ri_channel_param_t) { .add_msgs = 0, .msg_size = sizeof(msg_response_t), },
  (ri_channel_param_t) { .add_msgs = 10, .msg_size = sizeof(msg_event_t), },
  { 0 },
};



typedef struct client {
    ri_producer_t *command;
    ri_consumer_t *response;
    ri_consumer_t *event;
} client_t;


typedef struct server {
    int fd;
    ri_consumer_t *command;
    ri_producer_t *response;
    ri_producer_t *event;
} server_t;


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

static void dump_msg_command(const msg_command_t *msg)
{
  printf("command: \tid=0x%x\n", msg->id);
  printf("\targ[0]=0x%x\n", msg->args[0]);
  printf("\targ[1]=0x%x\n", msg->args[1]);
  printf("\targ[2]=0x%x\n", msg->args[2]);
}


static void dump_msg_response(const msg_response_t *msg)
{
  printf("response: \tid=0x%x\n", msg->id);
  printf("\tresult=%i\n", msg->result);
  printf("\tdata[1]=0x%x\n", msg->data);
}


static void dump_msg_event(const msg_event_t *msg)
{
  printf("event: \tid=%u\n", msg->id);
  printf("\tnr=%u\n", msg->nr);
}


static void client_delete(client_t *client)
{
  if (client->command)
    ri_producer_delete(client->command);
  if (client->response)
    ri_consumer_delete(client->response);
  if (client->event)
    ri_consumer_delete(client->event);
  free(client);
}


static client_t* client_new(int socket, const ri_channel_param_t *consumers, const ri_channel_param_t *producers)
{
  client_t *client = calloc(1, sizeof(client_t));

  if (!client)
    goto fail_alloc;

  ri_vector_t *vec = ri_vector_new(consumers, producers, NULL);

  if (!vec)
    goto fail_vec;

  int r = ri_vector_send(vec, socket);

  if (r < 0)
    goto fail_send;

  client->command = ri_vector_take_producer(vec, 0);
  if (!client->command)
    goto fail_channel;

  client->response = ri_vector_take_consumer(vec, 0);
  if (!client->response)
    goto fail_channel;

  client->event = ri_vector_take_consumer(vec, 1);
  if (!client->event)
    goto fail_channel;

  ri_vector_delete(vec);
  return client;

fail_channel:
fail_send:
  ri_vector_delete(vec);
fail_vec:
  client_delete(client);
fail_alloc:
  return NULL;
}

static void server_delete(server_t* server)
{
  if (server->command)
    ri_consumer_delete(server->command);
  if (server->response)
    ri_producer_delete(server->response);
  if (server->event)
    ri_producer_delete(server->event);
  free(server);
}

static server_t* server_new(int socket)
{
  server_t *server = calloc(1, sizeof(server_t));

  if (!server)
    goto fail_alloc;

  ri_vector_t *vec = ri_vector_receive(socket);

  if (!vec)
    goto fail_recv;

  server->command = ri_vector_take_consumer(vec, 0);
  if (!server->command)
    goto fail_channel;

  server->response = ri_vector_take_producer(vec, 0);
  if (!server->response)
    goto fail_channel;

  server->event = ri_vector_take_producer(vec, 1);
  if (!server->event)
    goto fail_channel;

  ri_vector_delete(vec);
  return server;

fail_channel:
  ri_vector_delete(vec);
fail_recv:
  server_delete(server);
fail_alloc:
  return NULL;
}


void client_run(client_t *client, const msg_command_t *cmds)
{
  const msg_command_t *cmd = cmds;
  *(msg_command_t*)ri_producer_msg(client->command) = *cmd;
  ri_producer_force_push(client->command);

  for (;;) {
    usleep(10000);
    for (;;) {
      ri_consume_result_t r = ri_consumer_pop(client->response);

      if ((r == RI_CONSUME_RESULT_NO_MSG) || (r == RI_CONSUME_RESULT_NO_UPDATE))
        break;

      printf("client received:\n");
      dump_msg_response(ri_consumer_msg(client->response));
      if (cmd->id == CMDID_UNKNOWN)
        return;
      *(msg_command_t*)ri_producer_msg(client->command) = *cmd;
      ri_producer_force_push(client->command);
      cmd++;
    }

    for (;;) {
      ri_consume_result_t r = ri_consumer_pop(client->event);

      if ((r == RI_CONSUME_RESULT_NO_MSG) || (r == RI_CONSUME_RESULT_NO_UPDATE))
        break;

      printf("client received:\n");
      dump_msg_event(ri_consumer_msg(client->event));
    }
  }
}


static int32_t server_send_events(ri_producer_t *producer, uint32_t id, unsigned num, bool force)
{
  for (unsigned i = 0; i < num; i++) {
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

void server_run(server_t *server)
{

  for (int i = 0; i < MAX_CYCLES; i++) {
    usleep(10000);
    bool run = true;
    ri_consume_result_t r = ri_consumer_pop(server->command);

    if ((r == RI_CONSUME_RESULT_NO_MSG) || (r == RI_CONSUME_RESULT_NO_UPDATE))
      continue;

    const msg_command_t *cmd = ri_consumer_msg(server->command);
    printf("server received:\n");
    dump_msg_command(cmd);

    msg_response_t *rsp = ri_producer_msg(server->response);

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
      rsp->result = server_send_events(server->event, cmd->args[0], cmd->args[1], cmd->args[2]);
      break;
    case CMDID_DIV:
      rsp->result = server_div(cmd->args[0], cmd->args[1], &rsp->data);
      break;
    default:
      rsp->result = -1;
      break;
    }
    ri_producer_force_push(server->response);
    if (!run)
      break;
  }
}

static int sockets[2];

int server_start(void* arg)
{

  server_t* server = server_new(sockets[0]);

  if (!server) {
    return -1;
  }

  server_run(server);

  printf("deleting server\n");
  server_delete(server);

  return 0;
}



int client_start(void* arg)
{
  client_t* client = client_new(sockets[1], server2client_channels, client2server_channels);

  if (!client) {
    return -1;
  }

  client_run(client, commands);

  printf("deleting client\n");
  client_delete(client);

  return 0;
}



int main() {
  socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sockets);
  thrd_t server_thread;
  thrd_t client_thread;

  int r = thrd_create( &server_thread, server_start, NULL);
  if (r != thrd_success) {
    return -1;
  }

  r = thrd_create( &client_thread, client_start, NULL);
  if (r != thrd_success) {
    return -1;
  }

  thrd_join(server_thread, NULL);
  thrd_join(client_thread, NULL);
}
