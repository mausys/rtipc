#pragma once

int ipc_create_socket_pair(int sockets[2]);
int ipc_send_fd(int socket, int fd);
int ipc_receive_fd(int socket);
