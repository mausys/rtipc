#include <rtipc.h>

#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <threads.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "channels.h"

#define ARRAY_LEN 100

typedef enum {
    CMD_SET_U8 = 111,
    CMD_SET_U16,
    CMD_SET_U32,
    CMD_SET_F64,
    CMD_SET_ARRAY,
} cmd_id_t;


typedef struct {
    channel_cmd_t cmd;
    channel_rsp_t rsp;
    channel_rpdo1_t rpdo1;
    channel_rpdo2_t rpdo2;
    channel_tpdo1_t tpdo;
} channels_t;

typedef struct {
    ri_shm_mapper_t *shm;
    ri_producer_mapper_t *cmd;
    ri_consumer_mapper_t *rsp;
    ri_producer_mapper_t *rpdo1;
    ri_producer_mapper_t *rpdo2;
    ri_consumer_mapper_t *tpdo;
    uint32_t arg;
    uint8_t idx;
    channels_t channels;
} client_t;


typedef struct {
    ri_shm_mapper_t *shm;
    ri_consumer_mapper_t *cmd;
    ri_producer_mapper_t *rsp;
    ri_consumer_mapper_t *rpdo1;
    ri_consumer_mapper_t *rpdo2;
    ri_producer_mapper_t *tpdo;
    channels_t channels;
} server_t;


static uint32_t now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}


static int sendfd(int socket, int fd)
{
    uint8_t buf[CMSG_SPACE(sizeof(fd))] = { 0 };

    struct msghdr msghdr = {
        .msg_iov = NULL,
        .msg_iovlen = 0,
        .msg_control = buf,
        .msg_controllen = CMSG_SPACE(sizeof(fd)),
    };

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msghdr);

    *cmsg = (struct cmsghdr) {
        .cmsg_level = SOL_SOCKET,
        .cmsg_type = SCM_RIGHTS,
        .cmsg_len = CMSG_LEN(sizeof(fd)),
    };

    *((int*)CMSG_DATA(cmsg)) = fd;

    return sendmsg(socket, &msghdr, 0);
}


static int recvfd(int socket)  // receive fd from socket
{
    int fd;
    uint8_t cmsg[CMSG_SPACE(sizeof(fd))];

    struct msghdr msghdr = {
        .msg_iov = NULL,
        .msg_iovlen = 0,
        .msg_control = cmsg,
        .msg_controllen = sizeof(cmsg),
    };

    int r = recvmsg(socket, &msghdr, 0);

    if (r < 0)
        return r;

    struct cmsghdr *cmsghdr = CMSG_FIRSTHDR(&msghdr);

    fd = *(int*)CMSG_DATA(cmsghdr);

    return fd;
}



static void set_header(header_t *header)
{
    ++header->seqno;
    header->timestamp = now();
}

static void server_set_rsp(server_t *server)
{
    server->channels.rsp.header->seqno = server->channels.rsp.header->seqno;
    server->channels.rsp.header->timestamp = now();
    *server->channels.rsp.cmd = 0;
}


static void server_process(server_t *server)
{
    int r = ri_consumer_mapper_update(server->cmd);

    if (r != 1)
        return;

    switch (*server->channels.cmd.cmd) {
        default:
            printf("unknown cmd received %u\n", *server->channels.cmd.cmd);
            break;
    }

    server_set_rsp(server);
    ri_producer_mapper_update(server->rsp);
}


static void client_set_cmd(client_t *client)
{
    set_header(client->channels.cmd.header);

    switch (*client->channels.cmd.cmd) {

        default:
        case CMD_SET_U8:

            break;
    }

        ri_producer_mapper_update(client->cmd);
}


static void client_check_data(client_t *client)
{
    switch (*client->channels.cmd.cmd) {
        default:
            printf("client_check_data unknown cmd_id\n");
            break;
    }
}


static void client_process(client_t *client)
{
    int r = ri_consumer_mapper_update(client->rsp);

    if (r != 1)
        return;

    //printf("client_task rx=%u\n", client->rx.rsp->header.seqno);

    if (client->channels.cmd.header->seqno == client->channels.rsp.header->seqno + 1)
        return;
    else if (client->channels.rsp.header->seqno == client->channels.cmd.header->seqno) {
        client_check_data(client);
        client->arg++;
        (*client->channels.cmd.cmd)++;
        client->idx = (client->idx + 1) % ARRAY_LEN;
        client_set_cmd(client);
    } else {
        printf("client_task %u %u\n", client->channels.cmd.header->seqno, client->channels.rsp.header->seqno);
    }
}



static client_t *client_new(int socket)
{
    int fd = recvfd(socket);

    if (fd < 0) {
        printf("recvfd failed\n");
        return NULL;
    }


    client_t *client = calloc(1, sizeof(client_t));

    if (!client)
        return NULL;


    ri_object_t cmd_objects[16];
    ri_object_t rsp_objects[16];
    ri_object_t rpdo1_objects[16];
    ri_object_t rpdo2_objects[16];
    ri_object_t tpdo1_objects[16];

    channels_t *channels = &client->channels;

    channel_cmd_to_objects(&channels->cmd, cmd_objects);
    channel_rsp_to_objects(&channels->rsp, rsp_objects);
    channel_rpdo1_to_objects(&channels->rpdo1, rpdo1_objects);
    channel_rpdo2_to_objects(&channels->rpdo2, rpdo2_objects);
    channel_tpdo1_to_objects(&channels->tpdo, tpdo1_objects);


    ri_object_t *consumer_objects[] = {rsp_objects, tpdo1_objects, NULL};
    ri_object_t *producer_objects[] = {cmd_objects, rpdo1_objects, rpdo2_objects, NULL};

    client->shm = ri_client_shm_mapper_new(fd, consumer_objects, producer_objects);

    if (!client->shm)
        goto fail_shm;

    client->cmd = ri_shm_mapper_get_producer(client->shm, 0);
    client->rpdo1 = ri_shm_mapper_get_producer(client->shm, 1);
    client->rpdo2 = ri_shm_mapper_get_producer(client->shm, 2);

    client->rsp = ri_shm_mapper_get_consumer(client->shm, 0);
    client->tpdo = ri_shm_mapper_get_consumer(client->shm, 1);

    return client;


fail_cos:
    ri_shm_mapper_delete(client->shm);
fail_shm:
    free(client);
    return NULL;
}


static server_t *server_new(int socket)
{
    int r;
    server_t *server = calloc(1, sizeof(server_t));

    if (!server)
        return NULL;

    ri_object_t cmd_objects[16];
    ri_object_t rsp_objects[16];
    ri_object_t rpdo1_objects[16];
    ri_object_t rpdo2_objects[16];
    ri_object_t tpdo1_objects[16];

    channels_t *channels = &server->channels;

    channel_cmd_to_objects(&channels->cmd, cmd_objects);
    channel_rsp_to_objects(&channels->rsp, rsp_objects);
    channel_rpdo1_to_objects(&channels->rpdo1, rpdo1_objects);
    channel_rpdo2_to_objects(&channels->rpdo2, rpdo2_objects);
    channel_tpdo1_to_objects(&channels->tpdo, tpdo1_objects);

    ri_object_t *producer_objects[] = {rsp_objects, tpdo1_objects, NULL};
    ri_object_t *consumer_objects[] = {cmd_objects, rpdo1_objects, rpdo2_objects, NULL};


    server->shm = ri_server_anon_shm_mapper_new(consumer_objects, producer_objects);

    server->cmd = ri_shm_mapper_get_consumer(server->shm, 0);
    server->rpdo1 = ri_shm_mapper_get_consumer(server->shm, 1);
    server->rpdo2 = ri_shm_mapper_get_consumer(server->shm, 2);

    server->rsp = ri_shm_mapper_get_producer(server->shm, 0);
    server->tpdo = ri_shm_mapper_get_producer(server->shm, 1);

    //ri_producer_mapper_dump(server->pos);
    //ri_consumer_mapper_dump(server->cos);
    int fd = ri_shm_get_fd(ri_shm_mapper_get_shm(server->shm));

    r = sendfd(socket, fd);

    if (r < 0) {
        printf("sendfd failed\n");
        goto fail;
    }

    return server;
fail:
    free(server);
    return NULL;
}


static void client_delete(client_t *client)
{

    ri_shm_mapper_delete(client->shm);
    free(client);
}



static void server_delete(server_t *server)
{

    ri_shm_mapper_delete(server->shm);
    free(server);
}


static void client_task(int socket)
{
    client_t *client = client_new(socket);

    close(socket);

    if (!client) {
        printf("create client failed\n");
        return;
    }

    client_set_cmd(client);

    for (int i = 0; i < 10000; i++) {
        client_process(client);
        usleep(100);
    }

    printf("client_task arg=%u\n", client->arg);

    client_delete(client);
}


static void server_task(int socket)
{
    server_t *server = server_new(socket);

    close(socket);

    if (!server) {
        printf("create server failed\n");
        return;
    }

    for (int i = 0; i < 100000; i++) {
        server_process(server);
        usleep(10);
    }

    server_delete(server);
}


int thrd_server_entry(void *ud)
{
    int fd = *(int*)ud;
    server_task(fd);
    return 0;
}

int thrd_client_entry(void *ud)
{
    int fd = *(int*)ud;
    client_task(fd);
    return 0;
}


static void threads(int fd_server, int fd_client)
{
    thrd_t thrd_server;
    thrd_t thrd_client;
    thrd_create(&thrd_server, thrd_server_entry, &fd_server);

    thrd_create(&thrd_client, thrd_client_entry, &fd_client);

    thrd_join(thrd_server, NULL);
    thrd_join(thrd_client, NULL);
}


static void processes(int fd_server, int fd_client)
{
    pid_t pid = fork();

    if (pid == 0) {
        close(fd_client);
        server_task(fd_server);
    } else if (pid >= 0) {
        close(fd_server);
        client_task(fd_client);
    } else {
        printf("fork failed\n");
    }
}



int main(void)
{
    bool use_threads = true;

    int sv[2];
    int r =  socketpair(AF_UNIX, SOCK_DGRAM, 0, sv);

    if (r < 0) {
        printf("socketpair failed\n");
        return -1;
    }

    if (use_threads)
        threads(sv[0], sv[1]);
    else
        processes(sv[0], sv[1]);

    return 0;
}
