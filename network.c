#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "network.h"
#include "logger.h"

#define BACKLOG 10

static int *stop_network;

static int addr_str2bin(char address[], int port, struct sockaddr_storage *sa)
{
    int res;
    struct sockaddr_in *addr_v4;
    struct sockaddr_in6 *addr_v6;

    addr_v4 = (struct sockaddr_in *)sa;
    addr_v4->sin_family = AF_INET;
    addr_v4->sin_port = htons(port);

    res = inet_pton(addr_v4->sin_family, address, &addr_v4->sin_addr);

    if (res == 1)
	return 0;

    addr_v6 = (struct sockaddr_in6 *)sa;
    addr_v6->sin6_family = AF_INET6;
    addr_v6->sin6_port = htons(port);

    res = inet_pton(addr_v6->sin6_family, address, &addr_v6->sin6_addr);

    if (res == 1)
	return 0;

    fprintf(stderr, "[ERROR][NETWORK] addr_str2bin\n");

    return -1;
}

static int addr_bin2str(struct sockaddr_storage *sa, char address[], int len)
{
    struct in_addr ipv4_addr;
    struct in6_addr ipv6_addr;

    ipv4_addr = ((struct sockaddr_in *)sa)->sin_addr;

    if (inet_ntop(AF_INET, &ipv4_addr, address, len))
	return 0;

    ipv6_addr = ((struct sockaddr_in6 *)sa)->sin6_addr;

    if (inet_ntop(AF_INET6, &ipv6_addr, address, len))
	return 0;

    log_message(LOG_LEVEL_ERROR, "addr_bin2str");
    return -1;
}

int create_listener(int port, char *address, int *stop_network_flag)
{
    int server_sock_fd = -1;
    struct sockaddr_storage sa;

    if (addr_str2bin(address, port, &sa))
	return -1;

    if ((server_sock_fd = socket(sa.ss_family, SOCK_STREAM, 0)) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "socket creation");
	goto Error;
    }

    if (bind(server_sock_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "binding");
	goto Error;
    }

    if (listen(server_sock_fd, BACKLOG) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "listening");
	goto Error;
    }

    stop_network = stop_network_flag;

    return server_sock_fd;

Error:
    if (server_sock_fd != -1)
	close(server_sock_fd);

    return -1;
}

int accept_connection(int server_sock_fd, char client_address[], int addr_len)
{
    int client_sock_fd;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_size = sizeof(struct sockaddr_storage);

    while ((client_sock_fd = accept(server_sock_fd,
	(struct sockaddr *)&peer_addr, &peer_addr_size)) == -1)
    {
	if (errno == EINTR)
	{
	    if (*stop_network)
		return 0;
	    continue;
	}

	log_message(LOG_LEVEL_ERROR, "accepting");

	return -1;
    }

    if (addr_bin2str(&peer_addr, client_address, addr_len))
	return -1;

    return client_sock_fd;
}

int recv_request(void *client_sock_fd, char *buffer, int buffer_len)
{
    int buflen;

    while ((buflen = recv(*(int*)client_sock_fd, buffer, buffer_len, 0)) == -1)
    {	
	if (errno == EINTR)
	{
	    if (*stop_network)
		return 0;
	    continue;
	}

	log_message(LOG_LEVEL_ERROR, "recv");

	return -1;
    }

    return buflen;
}

int set_recv_timeout(void *client_sock_fd, int timeout)
{
    struct timeval tv;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    return setsockopt(*(int*)client_sock_fd, SOL_SOCKET, SO_RCVTIMEO,
	(const char*)&tv, sizeof tv);
}

int send_response(void *client_sock_fd, char *buffer, int buffer_len)
{
    return send(*(int*)client_sock_fd, buffer, buffer_len, 0);
}

int close_socket(int sock_fd)
{
    if (sock_fd != -1 && close(sock_fd))
    {
	log_message(LOG_LEVEL_ERROR, "socket closing");

	return -1;
    }

    return 0;
}
