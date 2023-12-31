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


#define ARRAY_LEN 100

typedef enum {
    CMD_SET_U8 = 111,
    CMD_SET_U16,
    CMD_SET_U32,
    CMD_SET_F64,
    CMD_SET_ARRAY,
} cmd_id_t;

typedef struct {
    uint32_t seqno;
    uint32_t timestamp;
} header_t;


typedef union {
    uint8_t u8;
    int8_t s8;
    uint16_t u16;
    int16_t s16;
    uint32_t u32;
    int32_t s32;
    uint64_t u64;
    int64_t s64;
    float f32;
    double f64;
} generic_t;


typedef struct
{
    header_t *header;
    int32_t *rsp;
    uint8_t *u8;
    uint16_t *u16;
    uint32_t *u32;
    uint32_t *array;
    double *f64;
} s2c_t;


typedef struct {
    header_t *header;
    uint32_t *cmd;
    generic_t *arg1;
    generic_t *arg2;
} c2s_t;


typedef struct {
    ri_shm_t *shm;
    ri_consumer_objects_t *cos;
    ri_producer_objects_t *pos;
    uint32_t arg;
    uint8_t idx;
    s2c_t rx;
    c2s_t tx;
} client_t;


typedef struct {
    ri_shm_t *shm;
    ri_consumer_objects_t *cos;
    ri_producer_objects_t *pos;
    c2s_t rx;
    s2c_t tx;
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


static void map_s2c(s2c_t *s2c, ri_object_t  objs[])
{
    ri_object_t tmp[] =  {
        RI_OBJECT(s2c->header),
        RI_OBJECT(s2c->rsp),
        RI_OBJECT(s2c->u8),
        RI_OBJECT(s2c->u16),
        RI_OBJECT(s2c->u32),
        RI_OBJECT_ARRAY(s2c->array, ARRAY_LEN),
        RI_OBJECT(s2c->f64),
        RI_OBJECT_END,
    };

    memcpy(objs, tmp, sizeof(tmp));
}


static void map_c2s(c2s_t *c2s, ri_object_t  objs[])
{
    ri_object_t tmp[] = {
        RI_OBJECT(c2s->header),
        RI_OBJECT(c2s->cmd),
        RI_OBJECT(c2s->arg1),
        RI_OBJECT(c2s->arg2),
        RI_OBJECT_END,
    };

    memcpy(objs, tmp, sizeof(tmp));
}


static void set_header(header_t *header)
{
    ++header->seqno;
    header->timestamp = now();
}

static void server_set_rsp(server_t *server)
{
    server->tx.header->seqno = server->rx.header->seqno;
    server->tx.header->timestamp = now();
    *server->tx.rsp = 0;
}


static void server_process(server_t *server)
{
    int r = ri_consumer_objects_update(server->cos);

    if (r != 1)
        return;

    switch (*server->rx.cmd) {
        case CMD_SET_U8:
            *server->tx.u8 = server->rx.arg1->u8;
            break;
        case CMD_SET_U16:
            *server->tx.u16 = server->rx.arg1->u16;
            break;
        case CMD_SET_U32:
            *server->tx.u32 = server->rx.arg1->u32;
            break;
        case CMD_SET_F64:
            *server->tx.f64 = server->rx.arg1->f64;
            break;
        case CMD_SET_ARRAY:
            server->tx.array[server->rx.arg2->u32] = server->rx.arg1->u32;
            break;
        default:
            printf("unknown cmd received %u\n", *server->rx.cmd);
            break;
    }

    server_set_rsp(server);
    ri_producer_objects_update(server->pos);
}


static void client_set_cmd(client_t *client)
{
    set_header(client->tx.header);

    switch (*client->tx.cmd) {
        case CMD_SET_U16:
            client->tx.arg1->u16 = client->arg;
            break;
        case CMD_SET_U32:
            client->tx.arg1->u32 =  client->arg;
            break;
        case CMD_SET_F64:
            client->tx.arg1->f64 = client->arg;
            break;
        case CMD_SET_ARRAY:
            client->tx.arg1->u32 = client->arg;
            client->tx.arg2->u32 = client->idx;
            break;
        default:
        case CMD_SET_U8:
            *client->tx.cmd = CMD_SET_U8;
            client->tx.arg1->u32 = client->arg;
            break;
    }

        ri_producer_objects_update(client->pos);
}


static void client_check_data(client_t *client)
{
    switch (*client->tx.cmd) {
        case CMD_SET_U8:
            if (*client->rx.u8 != (uint8_t)client->arg)
                printf("client_check_data [CMD_SET_U8] got wrong data: %u expected: %u\n", *client->rx.u8, (uint8_t)client->arg);
            break;
        case CMD_SET_U16:
            if (*client->rx.u16 != (uint16_t)client->arg)
                printf("client_check_data [CMD_SET_U16] got wrong data: %u expected: %u\n", *client->rx.u16, (uint16_t)client->arg);
            break;
        case CMD_SET_U32:
            if (*client->rx.u32 != (uint32_t)client->arg)
                printf("client_check_data [CMD_SET_U32] got wrong data: %u expected: %u\n", *client->rx.u32, (uint32_t)client->arg);
            break;
        case CMD_SET_F64:
            if (*client->rx.f64 != (double)client->arg)
                printf("client_check_data [CMD_SET_F64] got wrong data: %f expected: %f\n", *client->rx.f64, (double)client->arg);
            break;
        case CMD_SET_ARRAY:
            if (client->rx.array[client->idx] != (uint32_t)client->arg)
                printf("client_check_data [CMD_SET_ARRAY] got wrong data: %u expected: %u\n", client->rx.array[client->idx], (uint32_t)client->arg);
            break;
        default:
            printf("client_check_data unknown cmd_id\n");
            break;
    }
}


static void client_process(client_t *client)
{
    int r = ri_consumer_objects_update(client->cos);

    if (r != 1)
        return;

    //printf("client_task rx=%u\n", client->rx.rsp->header.seqno);

    if (client->tx.header->seqno == client->rx.header->seqno + 1)
        return;
    else if (client->tx.header->seqno == client->rx.header->seqno) {
        client_check_data(client);
        client->arg++;
        (*client->tx.cmd)++;
        client->idx = (client->idx + 1) % ARRAY_LEN;
        client_set_cmd(client);
    } else {
        printf("client_task %u %u\n", client->tx.header->seqno, client->rx.header->seqno);
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

    client->shm = ri_shm_map(fd);

    if (!client->shm)
        goto fail_shm;

    ri_object_t robjs[16];
    ri_object_t tobjs[16];

    map_c2s(&client->tx, tobjs);
    map_s2c(&client->rx, robjs);

    client->cos = ri_consumer_objects_new(client->shm, 0, robjs);

    if (!client->cos) {
        printf("client_new ri_consumer_objects_new failed\n");
        goto fail_cos;
    }

    client->pos = ri_producer_objects_new(client->shm, 0, tobjs, true);

    if (!client->pos) {
        printf("client_new ri_producer_objects_new failed\n");
        goto fail_pos;
    }

    return client;

fail_pos:
    ri_consumer_objects_delete(client->cos);
fail_cos:
    ri_shm_delete(client->shm);
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

    ri_object_t robjs[16];
    ri_object_t tobjs[16];

    map_c2s(&server->rx, robjs);
    map_s2c(&server->tx, tobjs);

    const ri_object_t *c2s_chns[] = {&robjs[0] , NULL};
    const ri_object_t *s2s_chns[] = {&tobjs[0] , NULL};

    server->shm = ri_objects_anon_shm_new(c2s_chns, s2s_chns);

    if (!server->shm)
        goto fail_shm;

    server->cos = ri_consumer_objects_new(server->shm, 0, robjs);

    if (!server->cos) {
        printf("server_new ri_consumer_objects_new failed\n");
        goto fail_cos;
    }

    server->pos = ri_producer_objects_new(server->shm, 0, tobjs, true);

    if (!server->pos) {
        printf("server_new ri_producer_objects_new failed\n");
        goto fail_pos;
    }

    int fd = ri_shm_get_fd(server->shm);

    r = sendfd(socket, fd);

    if (r < 0) {
        printf("sendfd failed\n");
        goto fail_send;
    }

    return server;

fail_send:
    ri_producer_objects_delete(server->pos);
fail_pos:
    ri_consumer_objects_delete(server->cos);
fail_cos:
    ri_shm_delete(server->shm);
fail_shm:
    free(server);
    return NULL;
}


static void client_delete(client_t *client)
{
    ri_consumer_objects_delete(client->cos);
    ri_producer_objects_delete(client->pos);
    ri_shm_delete(client->shm);
    free(client);
}



static void server_delete(server_t *server)
{
    ri_consumer_objects_delete(server->cos);
    ri_producer_objects_delete(server->pos);
    ri_shm_delete(server->shm);
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
