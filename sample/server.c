#include "server.h"

#include <unistd.h>
#include <errno.h>


#include <rtipc/rtipc.h>
#include <rtipc/odb.h>
#include "ipc.h"
#include "timestamp.h"

#include "common.h"
#include "myobjects.h"

#include <stdlib.h>

typedef struct {
    ri_shm_mapper_t *shm_mapper;
    ri_producer_mapper_t *response_mapper;
} server_t;

typedef struct {
    object_type_t type;
    uint8_t len;
} object_def_t;

static const object_def_t rpdo1[] = {
    { OBJECT_TYPE_U8, 1 },
    { OBJECT_TYPE_U64, 1 },
    { OBJECT_TYPE_I16, 1 },
    { OBJECT_TYPE_F64, 1 },
    { OBJECT_TYPE_U16, 1 },
    { OBJECT_TYPE_U8, 1 },
    { OBJECT_TYPE_U64, 1 },
    { OBJECT_TYPE_I16, 1 },
    { OBJECT_TYPE_F64, 1 },
    { OBJECT_TYPE_U16, 1 },
    { OBJECT_TYPE_U8, 1 },
    { OBJECT_TYPE_U64, 1 },
    { OBJECT_TYPE_I16, 1 },
    { OBJECT_TYPE_F64, 1 },
    { OBJECT_TYPE_U16, 1 },
    {0, 0}
};


static const object_def_t rpdo2[] = {
    { OBJECT_TYPE_U8, 1 },
    { OBJECT_TYPE_U64, 1 },
    { OBJECT_TYPE_I16, 1 },
    { OBJECT_TYPE_F64, 1 },
    { OBJECT_TYPE_U16, 1 },
    {0, 0}
};


static const object_def_t tpdo1[] = {
    { OBJECT_TYPE_F64, 1 },
    { OBJECT_TYPE_U8, 1 },
    { OBJECT_TYPE_I64, 1 },
    { OBJECT_TYPE_U32, 1 },
    {0, 0}
};


static const object_def_t tpdo2[] = {
    { OBJECT_TYPE_U32, 1 },
    { OBJECT_TYPE_U8, 1 },
    { OBJECT_TYPE_F64, 255 },
    { OBJECT_TYPE_U16, 1 },
    {0, 0}
};


static const object_def_t tpdo3[] = {
    { OBJECT_TYPE_U32 , 1},
    { OBJECT_TYPE_U64 , 1},
    { OBJECT_TYPE_U64 , 1},
    {0, 0}
};


static int init_consumer_vector(ri_consumer_vector_t* vec, const object_def_t *object, unsigned start_index)
{
    unsigned channel_index = ri_consumer_vector_get_index(vec);

    for (const object_def_t *it = object; it->len != 0; it++) {
        ri_object_meta_t meta = create_object_meta(it->type, channel_index, start_index, it->len);

        int r = ri_consumer_vector_add(vec, &meta);
        if (r < 0)
            return r;

        start_index++;
    }

    return start_index;
}


static int init_producer_vector(ri_producer_vector_t* vec, const object_def_t *object, unsigned start_index)
{
    unsigned channel_index = ri_producer_vector_get_index(vec);

    for (const object_def_t *it = object; it->len != 0; it++) {
        ri_object_meta_t meta = create_object_meta(it->type, channel_index, start_index, it->len);

        int r = ri_producer_vector_add(vec, &meta);
        if (r < 0)
            return r;

        start_index++;
    }
    return start_index;
}


static ri_odb_t *create_odb(void)
{
    ri_consumer_vector_t *consumers[3];
    ri_producer_vector_t *producers[4];

    ri_odb_t *odb = ri_odb_new(3, 4);

    for (unsigned i = 0; i < 3; i++) {
        consumers[i] = ri_odb_get_consumer_vector(odb, i);
    }


    for (unsigned i = 0; i < 4; i++) {
        producers[i] = ri_odb_get_producer_vector(odb, i);
    }

    ri_consumer_vector_add(consumers[0], &RI_OBJECT(command_t));
    ri_producer_vector_add(producers[0], &RI_OBJECT(response_t));

    int index = 1;

    index = init_consumer_vector(consumers[1], rpdo1, index);
    index = init_consumer_vector(consumers[2], rpdo2, index);

    index = init_producer_vector(producers[1], tpdo1, index);
    index = init_producer_vector(producers[2], tpdo2, index);
    index = init_producer_vector(producers[3], tpdo3, index);

    return odb;

}

static int await_command(ri_consumer_object_t *object, command_t *command)
{
    ri_consumer_mapper_t *mapper = ri_consumer_object_get_mapper(object);

    for (unsigned i = 0; i < 1000; i++) {
        int r = ri_consumer_mapper_update(mapper);

        if (r == 1) {
            return ri_consumer_object_copy(object, command);
        }

        usleep(1000);
    }

    return -ETIMEDOUT;
}


static int execute_command(server_t *server, const command_t *command, response_t *response)
{
    if (command->cmd == COMMAND_ID_GET) {
        ri_consumer_mapper_t *mapper = ri_shm_mapper_get_producer(server->shm_mapper, command->channel + 1);

        if (!mapper)
            return -EINVAL;

        ri_consumer_mapper_update(mapper);

        ri_consumer_object_t *object = ri_consumer_mapper_get_object(mapper, command->index);

        if (!object)
            return -EINVAL;

        generic_t v;
        ri_consumer_object_copy(object, &v);

        *response = (response_t) {
            .cookie = command->cookie,
            .timestamp = timestamp_now(),
            .value = v,
        };

    } else {

    }
}


void server_run(int socket)
{

    server_t *server = calloc(1, sizeof(server_t));

    if (!server)
        return;

    ri_odb_t *odb = create_odb();

    if (!odb)
        goto fail_alloc;

    ri_shm_mapper_t *shm_mapper = ri_odb_create_anon_shm(odb);

    if (!shm_mapper)
        goto fail_shm;

    ri_shm_t *shm_server = ri_shm_mapper_get_shm(server->shm_mapper);

    int fd = ri_shm_get_fd(shm_server);

    ipc_send_fd(socket, fd);


    ri_consumer_mapper_t *command_mapper = ri_shm_mapper_get_consumer(server->shm_mapper, 0);
    ri_producer_mapper_t *response_mapper = ri_shm_mapper_get_producer(server->shm_mapper, 0);


    ri_consumer_object_t *command_object = ri_consumer_mapper_get_object(command_mapper, 0);
    ri_producer_object_t *response_object = ri_producer_mapper_get_object(response_mapper, 0);


    for (;;) {
        command_t command;
        response_t response;
        int r = await_command(command_object, &command);
        if (r < 0)
            goto fail_command;

        r = execute_command(server, &command, &response);
        if (r < 0)
            goto fail_command;
    }

fail_command:
    ri_shm_mapper_delete(shm_mapper);
fail_shm:
    ri_odb_delete(odb);
fail_alloc:
    free(server);
}







static void server_delete(server_t *server)
{

    //ri_shm_mapper_delete(server->shm);
    free(server);
}
