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

#include "object.h"

typedef enum
{
    CMD_SET_U8 = 111,
    CMD_SET_U16,
    CMD_SET_U32,
    CMD_SET_F64,
} cmd_id_t;




typedef struct
{
    object_rsp_t *rsp;
    object_u8_t *u8;
    object_u16_t *u16;
    object_u32_t *u32;
    object_f64_t *f64;
} s2c_t;


typedef struct
{
    object_cmd_t *cmd;
    object_arg_t *arg1;
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
    bool connected;
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


static void set_header(object_header_t *header)
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

    if (!server->connected) {
        if (server->rx.cmd->header.seqno == 0)
            return;
        server->connected = true;
    }

    switch (server->rx.cmd->id) {
        case CMD_SET_U8:
            set_header(&server->tx.u8->header);
            server->tx.u8->data = server->rx.arg1->u8;
            break;
        case CMD_SET_U16:
            set_header(&server->tx.u16->header);
            server->tx.u16->data = server->rx.arg1->u16;
            break;
        case CMD_SET_U32:
            set_header(&server->tx.u32->header);
            server->tx.u32->data = server->rx.arg1->u32;
            break;
        case CMD_SET_F64:
            set_header(&server->tx.f64->header);
            server->tx.f64->data = server->rx.arg1->f64;
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
            client->tx.arg1->u16 = client->arg;
            break;
        case CMD_SET_U32:
            client->tx.arg1->u32 =  client->arg;
            break;
        case CMD_SET_F64:
            client->tx.arg1->f64 = client->arg;
            break;
        default:
        case CMD_SET_U8:
            client->tx.cmd->id = CMD_SET_U8;
            client->tx.arg1->u32 = client->arg;
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
    };

    rtipc_object_t client_tx_objs[] = {
        RTIPC_OBJECT_ITEM(client->tx.cmd),
        RTIPC_OBJECT_ITEM(client->tx.arg1),
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
    RTIPC_OBJECT_ITEM(server->rx.arg1),
    };

    rtipc_object_t server_tx_objs[] = {
        RTIPC_OBJECT_ITEM(server->tx.rsp),
        RTIPC_OBJECT_ITEM(server->tx.u8),
        RTIPC_OBJECT_ITEM(server->tx.u16),
        RTIPC_OBJECT_ITEM(server->tx.u32),
        RTIPC_OBJECT_ITEM(server->tx.f64),
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


static void client_delete(client_t *client)
{
    rtipc_delete(client->rtipc);
    free(client);
}



static void server_delete(server_t *server)
{
    rtipc_delete(server->rtipc);
    free(server);
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
    client_delete(client);
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

    for (int i = 0; i < 100000; i++) {
        server_process(server);
        usleep(10);
    }

    server_delete(server);
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
