#include <rtipc/server.h>
#include <rtipc/client.h>
#include <rtipc/log.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define ARRAY_LEN 100


typedef struct {
    ri_shm_t *shm;
    ri_producer_objects_t *pos;
    char *obj;
} client_t;


typedef struct {
    ri_shm_t *shm;
    ri_consumer_objects_t *cos;
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

    ri_object_t objs[] =  {
        RI_OBJECT_ARRAY(client->obj, ARRAY_LEN),
        RI_OBJECT_END,
    };

    ri_producer_t prd;


    r = ri_client_get_producer(client->shm, 0, &prd);

    if (r < 0) {
        LOG_ERR("client_new ri_client_get_consumer failed");
        goto fail_channel;
    }

    client->pos = ri_producer_objects_new(&prd, objs, false);
    if (!client->pos) {
        LOG_ERR("client_new ri_producer_objects_new failed");
        goto fail_pos;
    }

    return client;

fail_pos:
fail_channel:
    ri_shm_delete(client->shm);
fail_shm:
    free(client);
    return NULL;
}


static server_t *server_new(void)
{
    int r;
    server_t *server = calloc(1, sizeof(server_t));

    if (!server)
        return NULL;

    ri_object_t objs[] =  {
        RI_OBJECT_ARRAY(server->obj, ARRAY_LEN),
        RI_OBJECT_END,
    };

    const ri_object_t *chns[] = {
        objs,
        NULL,
    };

    server->shm = ri_server_create_anon_shm_for_objects(chns, NULL);

    if (!server->shm)
        goto fail_shm;

    ri_consumer_t cns;

    r = ri_server_get_consumer(server->shm, 0, &cns);

    if (r < 0) {
        LOG_ERR("server_new ri_server_get_consumer failed");
        goto fail_channel;
    }

    server->cos = ri_consumer_objects_new(&cns, objs);

    if (!server->cos) {
        LOG_ERR("server_new ri_consumer_objects_new failed");
        goto fail_cos;
    }

    return server;

fail_cos:
fail_channel:
    ri_shm_delete(server->shm);
fail_shm:
    free(server);
    return NULL;
}


static void client_delete(client_t *client)
{
    ri_producer_objects_delete(client->pos);

    ri_shm_delete(client->shm);

    free(client);
}



static void server_delete(server_t *server)
{
    ri_consumer_objects_delete(server->cos);

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

    snprintf(client->obj, ARRAY_LEN, "Hello Server\n");

    ri_producer_objects_update(client->pos);

    client_delete(client);
}


static void server_task(server_t *server)
{
    for (;;) {
        int r = ri_consumer_objects_update(server->cos);

        if (r == 1) {
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
