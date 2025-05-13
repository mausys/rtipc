#include <rtipc.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define BUFFER_SIZE 100


typedef struct {
    ri_rtipc_t *rtipc;
    ri_producer_t *prd;
    char *obj;
} client_t;


typedef struct {
    ri_rtipc_t *rtipc;
    ri_consumer_t *cns;
    char *obj;
} server_t;



static client_t *client_new(int fd)
{
    client_t *client = calloc(1, sizeof(client_t));

    if (!client)
        return NULL;

    client->rtipc = ri_rtipc_shm_map(fd);

    if (!client->rtipc)
        goto fail_rtipc;


    client->prd = ri_rtipc_get_producer(client->rtipc, 0);

    if (!client->prd) {
        printf("client_new ri_client_get_consumer failed\n");
        goto fail_channel;
    }

    client->obj = ri_producer_get_msg(client->prd);

    return client;

fail_channel:
    ri_rtipc_delete(client->rtipc);
fail_rtipc:
    free(client);
    return NULL;
}


static server_t *server_new(void)
{
    server_t *server = calloc(1, sizeof(server_t));

    if (!server)
        return NULL;

    ri_channel_size_t chns[] = { { .msg_size = BUFFER_SIZE}, { 0} };


    server->rtipc = ri_rtipc_anon_shm_new(chns, NULL);

    if (!server->rtipc)
        goto fail_rtipc;

    server->cns = ri_rtipc_get_consumer(server->rtipc, 0);

    if (!server->cns) {
        printf("server_new ri_server_get_consumer failed\n");
        goto fail_channel;
    }

    return server;

fail_channel:
    ri_rtipc_delete(server->rtipc);
fail_rtipc:
    free(server);
    return NULL;
}


static void client_delete(client_t *client)
{
    ri_rtipc_delete(client->rtipc);

    free(client);
}


static void server_delete(server_t *server)
{
    ri_rtipc_delete(server->rtipc);

    free(server);
}


static void client_task(int fd)
{
    client_t *client = client_new(fd);

    if (!client) {
        printf("server creation failed\n");
        return;
    }

    snprintf(client->obj, BUFFER_SIZE, "Hello Server\n");

    client->obj = ri_producer_force_put(client->prd, NULL);

    client_delete(client);
}


static void server_task(server_t *server)
{
    for (;;) {
        server->obj = ri_consumer_fetch_tail(server->cns);

        if (server->obj) {
            printf("%s\n", server->obj);
            break;
        }

        usleep(10000);
    }
}


int main(void)
{
    server_t *server = server_new();

    if (!server) {
        printf("server creation failed\n");
        return -1;
    }

    int fd = ri_rtipc_get_shm_fd(server->rtipc);

    pid_t pid = fork();

    if (pid >= 0) {
        client_task(fd);
    } else if (pid == 0) {
        server_task(server);
    } else {
        printf("fork failed\n");
    }

    server_delete(server);

    return 0;
}
