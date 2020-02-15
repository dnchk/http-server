#ifndef _NETWORK_H_
#define _NETWORK_H_

int create_listener(int port, char *address, int *stop_network);
int accept_connection(int server_sock_fd, char address[], int addr_len);
int recv_request(void *client_sock_fd, char *buffer, int buffer_len);
int set_recv_timeout(void *client_sock_fd, int timeout);
int send_response(void *client_sock_fd, char *buffer, int buffer_len);
int close_socket(int sock_fd);

#endif
