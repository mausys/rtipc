#include "client.h"

#include <stdlib.h>
#include <stdio.h>
#include <rtipc/mapper.h>


#include "ipc.h"

void client_run(int socket)
{
    int fd = ipc_receive_fd(socket);

    if (fd < 0)
        return;

    ri_shm_t *shm = ri_shm_map(fd);
    ri_shm_mapper_t *mapper_client = ri_shm_mapper_new(shm);
    ri_shm_mapper_dump(mapper_client);

}
