#include <rtipc.h>

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
    client_t *client = calloc(1, sizeof(client_t));

    if (!client)
        return NULL;

    client->shm = ri_shm_map(fd);

    if (!client->shm)
        goto fail_shm;

    ri_object_t objs[] =  {
        RI_OBJECT_ARRAY(client->obj, ARRAY_LEN),
        RI_OBJECT_END,
    };

    client->pos = ri_producer_objects_new(client->shm, 0, objs, false);
    if (!client->pos) {
        printf("client_new ri_producer_objects_new failed\n");
        goto fail_pos;
    }

    return client;

fail_pos:
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

    ri_object_t objs[] =  {
        RI_OBJECT_ARRAY(server->obj, ARRAY_LEN),
        RI_OBJECT_END,
    };

    const ri_object_t *chns[] = {
        objs,
        NULL,
    };

    server->shm = ri_objects_anon_shm_new(chns, NULL);

    if (!server->shm)
        goto fail_shm;

    server->cos = ri_consumer_objects_new(server->shm, 0, objs);

    if (!server->cos) {
        printf("server_new ri_consumer_objects_new failed\n");
        goto fail_cos;
    }

    return server;

fail_cos:
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
        printf("server creation failed\n");
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
        printf("server creation failed\n");
        return -1;
    }

    int fd = ri_shm_get_fd(server->shm);

    pid_t pid = fork();

    if (pid == 0) {
        client_task(fd);
    } else if (pid >= 0) {
        server_task(server);
    } else {
        printf("fork failed\n");
    }

    server_delete(server);

    return 0;
}
