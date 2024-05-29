#include "client.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <rtipc/mapper.h>

#include "myobjects.h"
#include "ipc.h"
#include "timestamp.h"

typedef struct {
    uint8_t channel;
    uint8_t index;
    uint8_t subindex;
} mux_t;


typedef struct {
    mux_t mux;
    command_id_t id;
} step_t;

static const step_t sequence[] = {
    { .mux = { .channel = 0, .index = 2, .subindex = 1 }, .id = COMMAND_ID_GET },
    { .mux = { .channel = 0, .index = 2, .subindex = 1 }, .id = COMMAND_ID_SET },
    { .mux = { .channel = 0, .index = 2, .subindex = 1 }, .id = COMMAND_ID_GET },
    { .id = COMMAND_ID_NOP },
};

typedef struct {
    ri_shm_mapper_t *shm_mapper;
    ri_producer_object_t *command;
    ri_consumer_object_t *response;
} client_t;


static int await_response(ri_consumer_object_t *object, response_t *response)
{
    ri_consumer_mapper_t *mapper = ri_consumer_object_get_mapper(object);

    for (unsigned i = 0; i < 1000; i++) {
        int r = ri_consumer_mapper_update(mapper);

        if (r == 1) {
            return ri_consumer_object_copy(object, response);
        }

        usleep(1000);
    }

    return -ETIMEDOUT;
}

static int execute_step(client_t *client, const step_t *step)
{

    command_t *command = ri_producer_object_get_pointer(client->command);

    if (step->id == COMMAND_ID_GET) {
        ri_producer_mapper_t *mapper = ri_shm_mapper_get_producer(client->shm_mapper, step->mux.channel + 1);

        if (!mapper)
           return -EINVAL;

        ri_producer_object_t *object = ri_producer_mapper_get_object(mapper, step->mux.index);

        if (!object)
           return -EINVAL;

        uint64_t v = 1234;
        
        ri_producer_object_copy(object, &v);

        ri_producer_mapper_update(mapper);
    }

    ri_producer_mapper_t *command_mapper = ri_producer_object_get_mapper(client->command);
    ri_producer_mapper_update(command_mapper);

    response_t response;
    await_response(client->response, &response);

    if (step->id == COMMAND_ID_SET)  {
           ri_consumer_mapper_t *mapper = ri_shm_mapper_get_consumer(client->shm_mapper, step->mux.channel + 1);

        if (!mapper)
           return -EINVAL;

        ri_consumer_object_t *object = ri_consumer_mapper_get_object(mapper, step->mux.index);

        if (!object)
           return -EINVAL;

        ri_consumer_mapper_update(mapper);

        uint64_t v;
        
        ri_consumer_object_copy(object, &v);
    }

}



void client_run(int socket)
{

    client_t *client = calloc(1, sizeof(client_t));

    if (!client)
        return;

    int fd = ipc_receive_fd(socket);

    if (fd < 0)
        goto fail_receive;

    ri_shm_t *shm = ri_shm_map(fd);

    if (!shm)
        goto fail_shm;

    client->shm_mapper = ri_shm_mapper_new(shm);

    if (!client->shm_mapper)
        goto fail_mapper;

    // shm mapper is now owner of shm
    shm = NULL;

    ri_producer_mapper_t *command_mapper = ri_shm_mapper_get_producer(client->shm_mapper, 0);

    if (!command_mapper)
        goto fail_command;

    ri_consumer_mapper_t *response_mapper = ri_shm_mapper_get_consumer(client->shm_mapper, 0);

    if (!response_mapper)
        goto fail_command;

    client->command = ri_producer_mapper_get_object(command_mapper, 0);

    if (!client->command)
        goto fail_command;

    client->response = ri_consumer_mapper_get_object(response_mapper, 0);

    if (!client->response)
        goto fail_command;


    for (const step_t *step = sequence; step->id != COMMAND_ID_NOP; step++) {

        execute_step(client, step);

        usleep(1000);
    }

fail_command:
    ri_shm_mapper_delete(client->shm_mapper);
fail_mapper:
    if (shm)
        ri_shm_delete(shm);
fail_shm:
    close(fd);
fail_receive:
    free(client);

}
