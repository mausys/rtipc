#include "server.h"

#include <rtipc/rtipc.h>
#include <rtipc/odb.h>
#include "ipc.h"

#include "common.h"

#include <stdlib.h>

typedef struct {
    ri_shm_mapper_t *shm_mapper;
    struct {
        ri_consumer_mapper_t *command;
        ri_producer_mapper_t *response;
        ri_consumer_mapper_t *rpdo1;
        ri_consumer_mapper_t *rpdo2;
        ri_producer_mapper_t *tpdo1;
        ri_producer_mapper_t *tpdo2;
        ri_producer_mapper_t *tpdo3;
    } channels;
} server_t;


static const ri_object_meta_t rpdo1[] = {
    RI_OBJECT_ID(0, uint8_t),
    RI_OBJECT_ID(0, uint64_t),
    RI_OBJECT_ID(0, int16_t),
    RI_OBJECT_ID(0, double),
};


static const ri_object_meta_t rpdo2[] = {
    RI_OBJECT_ID(0, uint8_t),
    RI_OBJECT_ID(0, uint64_t),
    RI_OBJECT_ID(0, int16_t),
    RI_OBJECT_ID(0, double),
    RI_OBJECT_ID(0, int16_t),
    RI_OBJECT_ID(0, int16_t),
    RI_OBJECT_END
};


static const ri_object_meta_t tpdo1[] = {
    RI_OBJECT_ID(0, double),
    RI_OBJECT_ID(0, uint8_t),
    RI_OBJECT_ID(0, int64_t),
    RI_OBJECT_ID(0, uint32_t),
    RI_OBJECT_END
};


static const ri_object_meta_t tpdo2[] = {
    RI_OBJECT_ID(0, uint32_t),
    RI_OBJECT_ID(0, uint8_t),
    RI_OBJECT_ARRAY_ID(0, double, 256),
    RI_OBJECT_ID(0, uint16_t),
    RI_OBJECT_END
};


static const ri_object_meta_t tpdo3[] = {
    RI_OBJECT_ID(0, uint32_t),
    RI_OBJECT_ID(0, uint64_t),
    RI_OBJECT_ID(0, uint64_t),
    RI_OBJECT_END
};


static int init_consumer_vector(ri_consumer_vector_t* vec, const ri_object_meta_t *object)
{
    for (const ri_object_meta_t *it = object; it->size != 0; it++) {
        int r = ri_consumer_vector_add(vec, it);
        if (r < 0)
            return r;
    }
    return 0;
}


static int init_producer_vector(ri_producer_vector_t* vec, const ri_object_meta_t *object)
{
    for (const ri_object_meta_t *it = object; it->size != 0; it++) {
        int r = ri_producer_vector_add(vec, it);
        if (r < 0)
            return r;
    }
    return 0;
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

    init_consumer_vector(consumers[1], rpdo1);
    init_consumer_vector(consumers[2], rpdo2);

    init_producer_vector(producers[1], tpdo1);
    init_producer_vector(producers[2], tpdo2);
    init_producer_vector(producers[3], tpdo3);

    return odb;

}


void server_run(int socket)
{

    server_t *server = calloc(1, sizeof(server_t));

    if (!server)
        return;

    ri_odb_t *odb = create_odb();

    if (!odb)
        return;


    server->shm_mapper = ri_odb_create_anon_shm(odb);

    // not needed anymore
    ri_odb_delete(odb);


    ri_shm_t *shm_server = ri_shm_mapper_get_shm(server->shm_mapper);

    int fd = ri_shm_get_fd(shm_server);

    ipc_send_fd(socket, fd);

    ri_consumer_mapper_t *command_channel = ri_shm_mapper_get_consumer(server->shm_mapper, 0);
    ri_consumer_object_t *command_object = ri_consumer_mapper_get_object(command_channel, 0);

    for (unsigned i = 0; i < 1000; i++) {
        int r = ri_consumer_mapper_update(command_channel);

        if (r == 1) {
            //command_t *command = ri_consumer_object_get_pointer(command_object);
        }
       // usleep(1000);

    }





}

static server_t* server_new(int socket)
{
    int r;


}






static void server_delete(server_t *server)
{

    //ri_shm_mapper_delete(server->shm);
    free(server);
}
