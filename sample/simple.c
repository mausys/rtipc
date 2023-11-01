#include <rtipc/om.h>
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
    ri_tom_t *tom;
    char *obj;
} client_t;


typedef struct {
    ri_shm_t *shm;
    ri_rom_t *rom;
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

    ri_obj_desc_t objs[] =  {
        RI_OBJECT_ARRAY(client->obj, ARRAY_LEN),
        RI_OBJECT_END,
    };

    ri_producer_t prd;


    r = ri_client_get_producer(client->shm, 0, &prd);

    if (r < 0) {
        LOG_ERR("client_new ri_client_get_consumer failed");
        goto fail_channel;
    }

    client->tom = ri_tom_new(&prd, objs, false);
    if (!client->tom) {
        LOG_ERR("client_new ri_tom_new failed");
        goto fail_tom;
    }

    return client;

fail_tom:
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

    ri_obj_desc_t objs[] =  {
        RI_OBJECT_ARRAY(server->obj, ARRAY_LEN),
        RI_OBJECT_END,
    };

    const ri_obj_desc_t *chns[] = {
        objs,
        NULL,
    };

    server->shm = ri_server_create_anon_shm(chns, NULL);

    if (!server->shm)
        goto fail_shm;

    ri_consumer_t cns;

    r = ri_server_get_consumer(server->shm, 0, &cns);

    if (r < 0) {
        LOG_ERR("server_new ri_server_get_consumer failed");
        goto fail_channel;
    }

    server->rom = ri_rom_new(&cns, objs);

    if (!server->rom) {
        LOG_ERR("server_new ri_rom_new failed");
        goto fail_rom;
    }

    return server;

fail_rom:
fail_channel:
    ri_shm_delete(server->shm);
fail_shm:
    free(server);
    return NULL;
}


static void client_delete(client_t *client)
{
    ri_tom_delete(client->tom);

    ri_shm_delete(client->shm);

    free(client);
}



static void server_delete(server_t *server)
{
    ri_rom_delete(server->rom);

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

    ri_tom_update(client->tom);

    client_delete(client);
}


static void server_task(server_t *server)
{
    for (;;) {
        int r = ri_rom_update(server->rom);

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
        LOG_ERR("fork failed");
    }

    server_delete(server);

    return 0;
}
