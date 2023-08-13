#include "rtipc.h"

#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

typedef struct
{
    uint32_t seqno;
    uint32_t timestamp;
} msg_header_t;


typedef enum
{
    CMD_SET_U8 = 111,
    CMD_SET_U16,
    CMD_SET_U32,
    CMD_SET_F64,
    CMD_SET_STR,
} cmd_id_t;


typedef union
{
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    double f64;
} cmd_arg_t;


typedef struct
{
    msg_header_t header;
    cmd_id_t id;
    cmd_arg_t arg;
} cmd_t;


typedef struct
{
    msg_header_t header;
    int ret;
} rsp_t;


typedef struct
{
    msg_header_t header;
    uint8_t data;
} data_u8_t;


typedef struct
{
    msg_header_t header;
    uint16_t data;
} data_u16_t;


typedef struct
{
    msg_header_t header;
    uint32_t data;
} data_u32_t;


typedef struct
{
    msg_header_t header;
    double data;
} data_double_t;


typedef struct
{
    msg_header_t header;
    char str[128];
} data_string_t;


typedef struct
{
    rsp_t *rsp;
    data_u8_t *u8;
    data_u16_t *u16;
    data_u32_t *u32;
    data_string_t *str;
    data_double_t *f64;
} s2c_t;


typedef struct
{
    cmd_t *cmd;
} c2s_t;


typedef struct
{
    rtipc_t *rtipc;
    uint32_t arg;
    s2c_t rx;
    c2s_t tx;
} client_t;


typedef struct
{
    rtipc_t *rtipc;
    uint32_t cmd_seqno;
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


static void set_header(msg_header_t *header)
{
    header->seqno++;
    header->timestamp = now();
}

static void server_set_rsp(server_t *server)
{
    server->tx.rsp->header.seqno = server->rx.cmd->header.seqno;
    server->tx.rsp->header.timestamp = now();
    server->tx.rsp->ret = 0;
}


static void server_process(server_t *server)
{
    int r = rtipc_recv(server->rtipc);

    if (r != 1)
        return;

    switch (server->rx.cmd->id) {
        case CMD_SET_U8:
            set_header(&server->tx.u8->header);
            server->tx.u8->data = server->rx.cmd->arg.u8;
            break;
        case CMD_SET_U16:
            set_header(&server->tx.u16->header);
            server->tx.u16->data = server->rx.cmd->arg.u16;
            break;
        case CMD_SET_U32:
            set_header(&server->tx.u32->header);
            server->tx.u32->data = server->rx.cmd->arg.u32;
            break;
        case CMD_SET_F64:
            set_header(&server->tx.f64->header);
            server->tx.f64->data = server->rx.cmd->arg.f64;
            break;
        case CMD_SET_STR:
            set_header(&server->tx.str->header);
            snprintf(server->tx.str->str, sizeof(server->tx.str->str), "%u", server->rx.cmd->arg.u32);
            break;
        default:
            error(-1, EINVAL, "unknown cmd received %u", server->rx.cmd->id);
            break;
    }

    server_set_rsp(server);
    rtipc_send(server->rtipc);
}


static void client_set_cmd(client_t *client)
{
    set_header(&client->tx.cmd->header);

    switch (client->tx.cmd->id) {
        case CMD_SET_U16:
            client->tx.cmd->arg.u16 = client->arg;
            break;
        case CMD_SET_U32:
            client->tx.cmd->arg.u32 =  client->arg;
            break;
        case CMD_SET_F64:
            client->tx.cmd->arg.f64 = client->arg;
            break;
        case CMD_SET_STR:
            client->tx.cmd->arg.u32 =  client->arg;
            break;
        default:
        case CMD_SET_U8:
            client->tx.cmd->id = CMD_SET_U8;
            client->tx.cmd->arg.u32 = client->arg;
            break;
    }
}


static void client_check_data(client_t *client)
{
    switch (client->tx.cmd->id) {
        case CMD_SET_U8:
            if (client->rx.u8->data != (uint8_t)client->arg)
                printf("client_check_data got wrong data: %u expected: %u\n", client->rx.u8->data, (uint8_t)client->arg);
            break;
        case CMD_SET_U16:
            if (client->rx.u16->data != (uint16_t)client->arg)
                printf("client_check_data got wrong data: %u expected: %u\n", client->rx.u16->data, (uint16_t)client->arg);
            break;
        case CMD_SET_U32:
            if (client->rx.u32->data != (uint32_t)client->arg)
                printf("client_check_data got wrong data: %u expected: %u\n", client->rx.u32->data, (uint32_t)client->arg);
            break;
        case CMD_SET_F64:
            if (client->rx.f64->data != (double)client->arg)
                printf("client_check_data got wrong data: %f expected: %f\n", client->rx.f64->data, (double)client->arg);
            break;
        case CMD_SET_STR:
            //TODO
            //printf("client_check_data got wrong data: %s %u\n", client->rx.str->str, client->arg);
            break;
        default:
            printf("client_check_data unknown cmd_id\n");
            break;
    }
}


static void client_process(client_t *client)
{
    if (client->tx.cmd->header.seqno == 0) {
        client_set_cmd(client);
        rtipc_send(client->rtipc);
        return;
    }

    int r = rtipc_recv(client->rtipc);

    if (r != 1)
        return;

    //printf("client_task rx=%u\n", client->rx.rsp->header.seqno);

    if (client->tx.cmd->header.seqno == client->rx.rsp->header.seqno + 1)
        return;
    else if (client->tx.cmd->header.seqno == client->rx.rsp->header.seqno) {
        client_check_data(client);
        client->arg++;
        client->tx.cmd->id++;
        client_set_cmd(client);
        rtipc_send(client->rtipc);
    } else {
        printf("client_task %u %u\n", client->tx.cmd->header.seqno, client->rx.rsp->header.seqno);
    }
}


static client_t *client_new(int fd)
{
    client_t *client = calloc(1, sizeof(client_t));

    rtipc_object_t client_rx_objs[] = {
        RTIPC_OBJECT_ITEM(client->rx.rsp),
        RTIPC_OBJECT_ITEM(client->rx.u8),
        RTIPC_OBJECT_ITEM(client->rx.u16),
        RTIPC_OBJECT_ITEM(client->rx.u32),
        RTIPC_OBJECT_ITEM(client->rx.f64),
        RTIPC_OBJECT_ITEM(client->rx.str),
    };

    rtipc_object_t client_tx_objs[] = {
        RTIPC_OBJECT_ITEM(client->tx.cmd),
    };

    client->rtipc = rtipc_client_new(fd, client_rx_objs, sizeof(client_rx_objs) / sizeof(client_rx_objs[0]),
                                     client_tx_objs, sizeof(client_tx_objs) / sizeof(client_tx_objs[0]));

    if (!client->rtipc)
        goto fail_rtipc;

    return client;

fail_rtipc:
    free(client);
    return NULL;
}


static server_t *server_new(void)
{
    server_t *server = calloc(1, sizeof(server_t));

    rtipc_object_t server_rx_objs[] = {
        RTIPC_OBJECT_ITEM(server->rx.cmd),
    };

    rtipc_object_t server_tx_objs[] = {
        RTIPC_OBJECT_ITEM(server->tx.rsp),
        RTIPC_OBJECT_ITEM(server->tx.u8),
        RTIPC_OBJECT_ITEM(server->tx.u16),
        RTIPC_OBJECT_ITEM(server->tx.u32),
        RTIPC_OBJECT_ITEM(server->tx.f64),
        RTIPC_OBJECT_ITEM(server->tx.str),
    };

    server->rtipc = rtipc_server_new(server_rx_objs, sizeof(server_rx_objs) / sizeof(server_rx_objs[0]),
                                     server_tx_objs, sizeof(server_tx_objs) / sizeof(server_tx_objs[0]));

    if (!server->rtipc)
        goto fail_rtipc;

    return server;

fail_rtipc:
    free(server);
    return NULL;
}


static void client_task(int socket)
{
    int fd = recvfd(socket);

    close(socket);

    if (fd < 0)
        error(fd, -fd, "recvfd failed");

    client_t *client = client_new(fd);

    if (!client)
        error(-1, 0, "create client failed");

    for (int i = 0; i < 10000; i++) {
        client_process(client);
        usleep(100);
    }

    printf("client_task arg=%u\n", client->arg);
}


static void server_task(int socket)
{
    server_t *server = server_new();

    if (!server) {
        error(-1, 0, "create server failed");
    }

    int fd = rtipc_get_fd(server->rtipc);

    int r = sendfd(socket, fd);

    close(socket);

    if (r < 0)
        error(r, -r, "sendfd failed");

    for (int i = 0; i < 10000; i++) {
        server_process(server);
        usleep(100);
    }
}


int main(void)
{
    int sv[2];
    int r =  socketpair(AF_UNIX, SOCK_DGRAM, 0, sv);

    if (r < 0)
         error(-1, errno, "socketpair failed");

   pid_t pid = fork();

    if (pid == 0) {
        close(sv[1]);
        server_task(sv[0]);
    } else if (pid != -1) {
        close(sv[0]);
        client_task(sv[1]);
    } else {
        error(errno, errno, "fork failed");
    }

    return 0;
}
