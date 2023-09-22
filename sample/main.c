#include <rtipc/object.h>
#include <rtipc/posix.h>

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


typedef enum {
    CMD_SET_U8 = 111,
    CMD_SET_U16,
    CMD_SET_U32,
    CMD_SET_F64,
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
    double *f64;
} s2c_t;


typedef struct {
    header_t *header;
    uint32_t *cmd;
    generic_t *arg1;
} c2s_t;


typedef struct {
    ri_shm_t *shm;
    ri_rom_t *rom;
    ri_tom_t *tom;
    uint32_t arg;
    s2c_t rx;
    c2s_t tx;
} client_t;


typedef struct {
    ri_shm_t *shm;
    ri_rom_t *rom;
    ri_tom_t *tom;
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
    int r = ri_rom_update(server->rom);

    if (r != 1)
        return;

    if (!server->connected) {
        if (server->rx.header->seqno == 0)
            return;
        server->connected = true;
    }

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
        default:
            error(-1, EINVAL, "unknown cmd received %u", *server->rx.cmd);
            break;
    }

    server_set_rsp(server);
    ri_tom_update(server->tom);
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
        default:
        case CMD_SET_U8:
            *client->tx.cmd = CMD_SET_U8;
            client->tx.arg1->u32 = client->arg;
            break;
    }
}


static void client_check_data(client_t *client)
{
    switch (*client->tx.cmd) {
        case CMD_SET_U8:
            if (*client->rx.u8 != (uint8_t)client->arg)
                printf("client_check_data got wrong data: %u expected: %u\n", *client->rx.u8, (uint8_t)client->arg);
            break;
        case CMD_SET_U16:
            if (*client->rx.u16 != (uint16_t)client->arg)
                printf("client_check_data got wrong data: %u expected: %u\n", *client->rx.u16, (uint16_t)client->arg);
            break;
        case CMD_SET_U32:
            if (*client->rx.u32 != (uint32_t)client->arg)
                printf("client_check_data got wrong data: %u expected: %u\n", *client->rx.u32, (uint32_t)client->arg);
            break;
        case CMD_SET_F64:
            if (*client->rx.f64 != (double)client->arg)
                printf("client_check_data got wrong data: %f expected: %f\n", *client->rx.f64, (double)client->arg);
            break;
        default:
            printf("client_check_data unknown cmd_id\n");
            break;
    }
}


static void client_process(client_t *client)
{
    if (client->tx.header->seqno == 0) {
        client_set_cmd(client);
        ri_tom_update(client->tom);
        return;
    }

    int r = ri_rom_update(client->rom);

    if (r != 1)
        return;

    //printf("client_task rx=%u\n", client->rx.rsp->header.seqno);

    if (client->tx.header->seqno == client->rx.header->seqno + 1)
        return;
    else if (client->tx.header->seqno == client->rx.header->seqno) {
        client_check_data(client);
        client->arg++;
        (*client->tx.cmd)++;
        client_set_cmd(client);
        ri_tom_update(client->tom);
    } else {
        printf("client_task %u %u\n", client->tx.header->seqno, client->rx.header->seqno);
    }
}

client_t *g_client;
server_t *g_server;

static client_t *client_new(int fd)
{
    client_t *client = calloc(1, sizeof(client_t));

    g_client = client;

    ri_obj_desc_t client_robjs[] = {
        RI_OBJECT_ITEM(client->rx.header),
        RI_OBJECT_ITEM(client->rx.rsp),
        RI_OBJECT_ITEM(client->rx.u8),
        RI_OBJECT_ITEM(client->rx.u16),
        RI_OBJECT_ITEM(client->rx.u32),
        RI_OBJECT_ITEM(client->rx.f64),
        RI_OBJECT_END,
    };

    ri_obj_desc_t client_tobjs[] = {
        RI_OBJECT_ITEM(client->tx.header),
        RI_OBJECT_ITEM(client->tx.cmd),
        RI_OBJECT_ITEM(client->tx.arg1),
        RI_OBJECT_END,
    };

    client->shm = ri_posix_shm_map(fd);

    ri_rchn_t rchn;
    ri_tchn_t tchn;

    size_t offset = ri_shm_get_rchn(client->shm, 0, &rchn);
    if (offset == 0)
        error(-1, 0, "client_new ri_shm_get_rchn failed");

    offset = ri_shm_get_tchn(client->shm, 0, &tchn);
    if (offset == 0)
        error(-1, 0, "client_new ri_shm_get_tchn failed");


    client->rom = ri_rom_new(&rchn, client_robjs);
    if (!client->rom)
        error(-1, 0, "client_new ri_rom_new failed");

    client->tom = ri_tom_new(&tchn, client_tobjs, true);
    if (!client->tom)
        error(-1, 0, "client_new ri_tom_new failed");



    return client;

fail_rtipc:
    free(client);
    return NULL;
}


static server_t *server_new(void)
{
    server_t *server = calloc(1, sizeof(server_t));
    g_server = server;

    ri_obj_desc_t server_robjs[] = {
        RI_OBJECT_ITEM(server->rx.header),
        RI_OBJECT_ITEM(server->rx.cmd),
        RI_OBJECT_ITEM(server->rx.arg1),
        RI_OBJECT_END,
    };

    ri_obj_desc_t server_tobjs[] = {
        RI_OBJECT_ITEM(server->tx.header),
        RI_OBJECT_ITEM(server->tx.rsp),
        RI_OBJECT_ITEM(server->tx.u8),
        RI_OBJECT_ITEM(server->tx.u16),
        RI_OBJECT_ITEM(server->tx.u32),
        RI_OBJECT_ITEM(server->tx.f64),
        RI_OBJECT_END,
    };

    size_t rbuf_size = ri_calc_buffer_size(server_robjs);
    size_t tbuf_size = ri_calc_buffer_size(server_tobjs);

    size_t rchn_size = ri_shm_calc_chn_size(rbuf_size);
    size_t tchn_size = ri_shm_calc_chn_size(tbuf_size);

    server->shm = ri_posix_shm_create(rchn_size + tchn_size);

    ri_rchn_t rchn;
    ri_tchn_t tchn;

    size_t offset = ri_shm_set_rchn(server->shm, 0, rbuf_size, false, &rchn);
    if (offset == 0)
        error(-1, 0, "server_new ri_shm_set_rchn failed");

    offset = ri_shm_set_tchn(server->shm, offset, tbuf_size, true, &tchn);
    if (offset == 0)
        error(-1, 0, "server_new ri_shm_set_tchn failed");

    server->rom = ri_rom_new(&rchn, server_robjs);
    if (!server->rom)
        error(-1, 0, "server_new ri_rom_new failed");

    server->tom = ri_tom_new(&tchn, server_tobjs, true);
    if (!server->tom)
        error(-1, 0, "server_new ri_tom_new failed");

    return server;

fail_rtipc:
    free(server);
    return NULL;
}


static void client_delete(client_t *client)
{
    ri_rom_delete(client->rom);
    ri_tom_delete(client->tom);
    ri_posix_shm_delete(client->shm);
    free(client);
}



static void server_delete(server_t *server)
{
    ri_rom_delete(server->rom);
    ri_tom_delete(server->tom);
    ri_posix_shm_delete(server->shm);
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

    int fd = ri_posix_shm_get_fd(server->shm);

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

int sv[2];

thrd_t thrd_server;
thrd_t thrd_client;

int main(void)
{
    int r =  socketpair(AF_UNIX, SOCK_DGRAM, 0, sv);

    thrd_create(&thrd_server, thrd_server_entry, &sv[0]);

    thrd_create(&thrd_client, thrd_client_entry, &sv[1]);

    thrd_join(thrd_server, NULL);
    thrd_join(thrd_client, NULL);

    return 0;
}
