#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "network.h"
#include "http.h"
#include "logger.h"
#include "config_parser.h"
#include "w3c_log.h"

#define CONFIG_FILENAME "config"
#define MAX_PORT_LEN 5

typedef struct {
    int port;
    char address[INET6_ADDRSTRLEN];
    char root[PATH_MAX];
    char w3c_log_path[PATH_MAX];
} config_ctx_t;

static config_ctx_t* read_config()
{
    config_parser_t *config_parser;
    char port[MAX_PORT_LEN];

    config_ctx_t *config_ctx = malloc(sizeof(config_ctx_t));
    if (!config_ctx)
    {
	log_message(LOG_LEVEL_ERROR, "config_ctx memory allocation");
	return NULL;
    }

    if (!(config_parser = config_parser_init(CONFIG_FILENAME)))
    {
	log_message(LOG_LEVEL_ERROR, "config parser initialization");
	goto Error;
    } 

    config_add_keyword(config_parser, "port", port, MAX_PORT_LEN);
    config_add_keyword(config_parser, "address", config_ctx->address,
	INET6_ADDRSTRLEN);
    config_add_keyword(config_parser, "root", config_ctx->root, PATH_MAX);
    config_add_keyword(config_parser, "w3c_log_path", config_ctx->w3c_log_path,
	PATH_MAX);

    if (config_parser_start(config_parser))
    {
	log_message(LOG_LEVEL_ERROR, "config: parsing");
	goto Error;
    }

    if (check_all_found(config_parser))
    {
	log_message(LOG_LEVEL_ERROR, "config: not all keywords found");
	goto Error;
    }

    config_ctx->port = atoi(port);
    if (config_ctx->port < 0 || config_ctx->port > 65536)
    {
	log_message(LOG_LEVEL_ERROR, "config: invalid port");
	goto Error;
    }

    config_parser_deinit(config_parser);

    return config_ctx;

Error:
    free(config_ctx);
    if (config_parser)
	config_parser_deinit(config_parser);

    return NULL;
}

sig_atomic_t stop_server;

static void handle_interrupt_sig(int sig)
{
    stop_server = 1;
    log_message(LOG_LEVEL_DEBUG, "interrupted signal received");
}

int main(void)
{
    int server_sock_fd = -1, client_sock_fd = -1, pid, status = 0, rv = 1;
    struct sigaction sa;
    http_ctx_t *http;
    config_ctx_t *config_ctx;
    w3c_log_field_t w3c_log_fields[] = {
	W3C_LOG_FIELD_C_IP,
	W3C_LOG_FIELD_CS_METHOD,
	W3C_LOG_FIELD_CS_URI,
	W3C_LOG_FIELD_SC_STATUS
    };
    char client_address[INET6_ADDRSTRLEN] = {};

    log_init(stdout, LOG_LEVEL_DEBUG);

    sa.sa_handler = handle_interrupt_sig;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        log_message(LOG_LEVEL_ERROR, "sigaction failed");
        goto Exit;
    }

    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
	log_message(LOG_LEVEL_ERROR, "signal function failed");
	goto Exit;
    }

    if (!(http = http_init()))
    {
	log_message(LOG_LEVEL_ERROR, "http initialization");
	goto Exit;
    }

    if (!(config_ctx = read_config()))
    {
	log_message(LOG_LEVEL_ERROR, "reading config");
	goto Exit;
    }

    http_set_root_folder(http, config_ctx->root);
    http_set_chunked(http, 1);

    http_set_callback(http, HTTP_CB_RECV, recv_request);
    http_set_callback(http, HTTP_CB_SET_RECV_TIMEOUT, set_recv_timeout);
    http_set_callback(http, HTTP_CB_SEND, send_response);

    if ((server_sock_fd = create_listener(config_ctx->port,
	config_ctx->address, &stop_server)) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "listener creation");
	goto Exit;
    }

    if (w3c_log_init(config_ctx->w3c_log_path, w3c_log_fields,
	(sizeof(w3c_log_fields) / sizeof(w3c_log_fields[0]))))
    {
	log_message(LOG_LEVEL_ERROR, "server_log initialization");
	goto Exit;
    }

    while(!stop_server)
    {
	if ((client_sock_fd = accept_connection(server_sock_fd, client_address,
	    INET6_ADDRSTRLEN)) < 1)
	{
	    if (!client_sock_fd)
		break;

	    log_message(LOG_LEVEL_ERROR, "connection acceptance");
	    goto Exit;
	}

	if ((pid = fork()))
	{
	    if (pid == -1)
	    {
		log_message(LOG_LEVEL_ERROR, "fork failed");
		goto Exit;
	    }

	    close_socket(client_sock_fd);

	    continue;
	}

	if ((http_handle_peer(http, client_address, &client_sock_fd) == -1))
	{
	    log_message(LOG_LEVEL_ERROR, "http_handle_peer");
	    goto Exit;
	}

	close_socket(client_sock_fd);

	log_message(LOG_LEVEL_DEBUG, "Child closed");
	return 0;
    }

    while ((pid = waitpid(-1, &status, 0)) > 0)
	log_message(LOG_LEVEL_DEBUG, "child terminated, pid:%d", pid);

    rv = 0;

Exit:
    free(config_ctx);

    close_socket(client_sock_fd);
    close_socket(server_sock_fd);

    http_deinit(http);

    w3c_log_deinit();
    log_deinit();

    return rv;
}
