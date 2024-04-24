#include <rtipc.h>

#include <unistd.h>
#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <threads.h>



#include "server.h"
#include "client.h"
#include "ipc.h"





int thrd_server_entry(void *ud)
{
    int fd = *(int*)ud;
    server_run(fd);
    return 0;
}

int thrd_client_entry(void *ud)
{
    int fd = *(int*)ud;
    client_run(fd);
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
        server_run(fd_server);
    } else if (pid >= 0) {
        close(fd_server);
        client_run(fd_client);
    } else {
        printf("fork failed\n");
    }
}



int main(void)
{
    bool use_threads = true;

    int sv[2];

    int r = ipc_create_socket_pair(sv);

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
