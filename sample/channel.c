#include <rtipc/server.h>
#include <rtipc/client.h>
#include <rtipc/log.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define BUFFER_SIZE 100


typedef struct {
    ri_shm_t *shm;
    ri_producer_t prd;
    char *obj;
} client_t;


typedef struct {
    ri_shm_t *shm;
    ri_consumer_t cns;
    char *obj;
} server_t;



static client_t *client_new(int fd)
{
    int r;

    client_t *client = calloc(1, sizeof(client_t));

    if (!client)
        return NULL;

    client->shm = ri_client_map_shm(fd);

    if (!client->shm)
        goto fail_shm;


    r = ri_client_get_producer(client->shm, 0, &client->prd);

    if (r < 0) {
        LOG_ERR("client_new ri_client_get_consumer failed");
        goto fail_channel;
    }

    client->obj = ri_producer_swap(&client->prd);

    return client;

fail_channel:
    ri_shm_delete(client->shm);
fail_shm:
    free(client);
    return NULL;
}


static server_t *server_new(void)
{
    server_t *server = calloc(1, sizeof(server_t));

    if (!server)
        return NULL;

    size_t chns[] = { ri_calc_channel_size(BUFFER_SIZE), 0};

    server->shm = ri_server_create_anon_shm_for_channels(chns, NULL);

    if (!server->shm)
        goto fail_shm;

    int r = ri_server_get_consumer(server->shm, 0, &server->cns);

    if (r < 0) {
        LOG_ERR("server_new ri_server_get_consumer failed");
        goto fail_channel;
    }

    return server;

fail_channel:
    ri_shm_delete(server->shm);
fail_shm:
    free(server);
    return NULL;
}


static void client_delete(client_t *client)
{
    ri_shm_delete(client->shm);

    free(client);
}


static void server_delete(server_t *server)
{
    ri_shm_delete(server->shm);

    free(server);
}


static void client_task(int fd)
{
    client_t *client = client_new(fd);

    if (!client) {
        LOG_ERR("server creation failed");
        return;
    }

    snprintf(client->obj, BUFFER_SIZE, "Hello Server\n");

    client->obj = ri_producer_swap(&client->prd);

    client_delete(client);
}


static void server_task(server_t *server)
{
    for (;;) {
        server->obj = ri_consumer_fetch(&server->cns);

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
        LOG_ERR("server creation failed");
        return -1;
    }

    int fd = ri_shm_get_fd(server->shm);

    pid_t pid = fork();

    if (pid == 0) {
        client_task(fd);
    } else if (pid >= 0) {
        server_task(server);
    } else {
        LOG_ERR("fork failed");
    }

    server_delete(server);

    return 0;
}
