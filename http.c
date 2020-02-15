#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include "utils.h"
#include "http.h"
#include "logger.h"
#include "w3c_log.h"

#define CHUNK_SIZE 1024
#define BUFSIZE 2048
#define MAX_MESSAGE_SIZE 1024

#define HTTP_VER "HTTP/1.1"
#define HTTP_LINE_END "\r\n"
#define HTTP_HDR_CONTENT_LENGTH "Content-Length"
#define HTTP_HDR_TRANSFER_ENCODING "Transfer-Encoding"
#define HTTP_HDR_CONNECTION "Connection"
#define HTTP_HDR_KEEPALIVE "Keep-Alive"

typedef struct {
    char *path;
    http_code_t http_code;
    int file_size;
} http_response_t;

static char *methods[HTTP_METHOD_UNKNOWN] = {
	[HTTP_METHOD_GET] = "GET",
	[HTTP_METHOD_HEAD] = "HEAD",
	[HTTP_METHOD_POST] = "POST",
	[HTTP_METHOD_PUT] = "PUT",
	[HTTP_METHOD_DELETE] = "DELETE",
	[HTTP_METHOD_CONNECT] = "CONNECT",
	[HTTP_METHOD_OPTIONS] = "OPTIONS",
	[HTTP_METHOD_TRACE] = "TRACE",
	[HTTP_METHOD_PATCH] = "PATCH"
};

static char* http_code2str(http_code_t http_code)
{
    switch (http_code)
    {
	case HTTP_CODE_OK:
	    return "OK";
	case HTTP_CODE_BAD_REQUEST:
	    return "Bad Request";
	case HTTP_CODE_NOT_FOUND:
	    return "Not Found";
	case HTTP_CODE_NOT_IMPLEMENTED:
	    return "Not Implemented";
    }

    return "";
}

static http_method_t http_method_str2code(char *buffer, int method_len)
{
    for (int i = 0; i < HTTP_METHOD_UNKNOWN; i++)
    {
	if (!strncmp(buffer, methods[i], method_len))
	    return i;
    }

    return HTTP_METHOD_UNKNOWN;
}

static char* http_method_code2str(http_method_t method)
{
    if (method >= HTTP_METHOD_GET && method <= HTTP_METHOD_PATCH)
	return methods[method];

    return "";
}

static int set_error_page(http_ctx_t *http_ctx, http_response_t *response)
{
    if (snprintf_with_alloc(&response->path, "%s/%d.html",
	http_ctx->root_folder, response->http_code) == -1)
    {
	return -1;
    }

    return 0;
}

static int file_exists(http_ctx_t *http_ctx, char *requested_file,
    http_response_t *response)
{
    int rv = 0;
    char *filepath = NULL;
    struct stat statbuf;

    if (snprintf_with_alloc(&filepath, "%s%s", http_ctx->root_folder,
	requested_file) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "snprintf_with_alloc failed");
	goto Exit;
    }

    if (access(filepath, F_OK) == -1)
    {
	log_message(LOG_LEVEL_DEBUG, "file doesn't exist");
	goto Exit;
    }

    if (stat(filepath, &statbuf) != 0)
    {
	log_message(LOG_LEVEL_WARNING, "stat() returned error");
	goto Exit;
    }

    if (S_ISDIR(statbuf.st_mode))
    {
	log_message(LOG_LEVEL_DEBUG, "requested file is directory");
	goto Exit;
    }

    response->path = filepath;
    response->file_size = statbuf.st_size;

    rv = 1;
Exit:
    return rv;
}

#define HTTP_STS_LINE_FMT "%s %d %s" HTTP_LINE_END
static int http_add_status_line(char **response_header,
    http_response_t *response)
{
    int len;
    char *http_code_str = http_code2str(response->http_code);

    if ((len = snprintf_with_alloc(response_header, HTTP_STS_LINE_FMT, HTTP_VER,
	response->http_code, http_code_str)) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "http_sprintf failed");
	return -1;
    }

    return len;
}

#define HTTP_HDR_FMT "%s: %s" HTTP_LINE_END
static int http_add_header(char **response_header, char *header, char *value)
{
    int len;

    if ((len = snprintf_with_alloc(response_header, HTTP_HDR_FMT, header,
	value)) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "http_sprintf failed");
	return -1;
    }

    return len;
}

static int http_add_header_end(char **response_header)
{
    int	len;

    if ((len = snprintf_with_alloc(response_header, "%s", HTTP_LINE_END)) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "http_sprintf failed");
	return -1;
    }

    return len;
}

static int parse_request_headers(char *buffer, http_request_t *request,
    http_ctx_t *http_ctx)
{
    char *line, *delim;

    line = strtok(buffer, HTTP_LINE_END);

    while (line)
    {
	delim = strstr(line, ":");

	if (!delim)
	    goto BadRequest;

	for (int i = 0; i < http_ctx->hdr_counter; ++i)
	{
	    if (!strncmp(http_ctx->hdr_handlers[i]->header, line, delim - line))
	    {
		for (int j = 0; delim; ++j)
		{
		    if (isalpha(*(delim + j)))
		    {
			if ((http_ctx->hdr_handlers[i]->handler(request,
			    delim + j, strlen(delim) - j)) == -1)
			{
			    goto BadRequest;
			}

			break;
		    }
		}
	    }
	}

	line = strtok(NULL, HTTP_LINE_END);
    }

    return 0;

BadRequest:
    log_message(LOG_LEVEL_DEBUG, "parsing header: bad request");
    return -1;
}

static int parse_request(char *buffer, int buffer_len, http_request_t *request,
    http_ctx_t *http_ctx)
{
    char *delimiter_ptr;
    int url_len;

    delimiter_ptr = strchr(buffer, ' ');

    if (delimiter_ptr)
	request->method = http_method_str2code(buffer, delimiter_ptr - buffer);
    else
	goto BadRequest;

    if (request->method == HTTP_METHOD_UNKNOWN)
    {
	log_message(LOG_LEVEL_DEBUG, "invalid http method");
	goto BadRequest;
    }

    buffer = delimiter_ptr + 1;
    delimiter_ptr = strchr(buffer, ' ');

    url_len = delimiter_ptr - buffer;
    if (url_len < 1)
	goto BadRequest;

    request->file = malloc(url_len + 1);

    /* TODO Send Internal Error in such case */
    if (!request->file)
	goto BadRequest;

    if (delimiter_ptr)
	strncpy(request->file, buffer, url_len);
    else
	goto BadRequest;

    request->file[url_len] = '\0';

    buffer = delimiter_ptr + 1;
    delimiter_ptr = strstr(buffer, HTTP_LINE_END);

    if (strncmp(buffer, "HTTP/1.1", delimiter_ptr - buffer))
    {
	log_message(LOG_LEVEL_DEBUG, "invalid http version");
	goto BadRequest;
    }
    /* TODO validate headers format */
    delimiter_ptr = strstr(buffer, HTTP_LINE_END HTTP_LINE_END);

    if (!delimiter_ptr)
    {
	log_message(LOG_LEVEL_DEBUG, "Parsing http request: invalid end");
	goto BadRequest;
    }

    delimiter_ptr = strstr(buffer, HTTP_LINE_END);
    buffer = delimiter_ptr + 1;

    if (parse_request_headers(buffer, request, http_ctx))
	goto BadRequest;

    return 0;

BadRequest:
    return -1;
}

/* Format: <chunk_len><CRLF><chunk><CRLF>...<0><CRLF><CRLF> */
#define CHUNK_FMT "%x" HTTP_LINE_END "%s" HTTP_LINE_END
static int send_chunk(http_ctx_t *http_ctx, void *net_ctx, char *chunk,
    int chunk_len)
{
    char *chunk_with_len = NULL;
    int len;

    if ((len = snprintf_with_alloc(&chunk_with_len, CHUNK_FMT, chunk_len,
	chunk)) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "snprintf_with_alloc failed");
	goto Error;
    }

    if ((http_ctx->send(net_ctx, chunk_with_len, len)) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "sending chunk: message");
	free(chunk_with_len);
	goto Error;
    }

    free(chunk_with_len);
    return 0;

Error:
    return -1;
}

static int send_chunked(http_ctx_t *http_ctx, void *net_ctx, int fd)
{
    char chunk[CHUNK_SIZE] = {};
    int buflen;

    while((buflen = read(fd, chunk, CHUNK_SIZE - 1)))
    {
	if (buflen < 0)
	{
	    log_message(LOG_LEVEL_ERROR, "Read failed");
	    goto Exit;
	}

	chunk[buflen] = '\0';

	if (send_chunk(http_ctx, net_ctx, chunk, buflen))
	{
	    log_message(LOG_LEVEL_ERROR, "sending message");
	    goto Exit;
	}
    }

    if (send_chunk(http_ctx, net_ctx, "", 0))
    {
	log_message(LOG_LEVEL_ERROR, "sending message last chunk");
	goto Exit;
    }

    return 0;

Exit:
    return -1;
}

static int send_not_chunked(http_ctx_t *http_ctx, void *net_ctx, int fd)
{
    char buf[MAX_MESSAGE_SIZE] = {};
    int buflen;

    while((buflen = read(fd, buf, MAX_MESSAGE_SIZE)))
    {
	if (buflen < 0)
	{
	    log_message(LOG_LEVEL_ERROR, "Read failed");
	    goto Exit;
	}

	if (http_ctx->send(net_ctx, buf, buflen) == -1)
	{
	    log_message(LOG_LEVEL_ERROR, "http_ctx->send(html)");
	    goto Exit;
	}
    }

    return 0;

Exit:
    return -1;
}

static int create_response(http_ctx_t *http_ctx, http_request_t *request,
    http_response_t *response)
{
    response->path = NULL;

    if (response->http_code != HTTP_CODE_BAD_REQUEST)
    {
	switch (request->method)
	{
	    case HTTP_METHOD_GET:
		if (!file_exists(http_ctx, request->file, response))
		    response->http_code = HTTP_CODE_NOT_FOUND;
		else
		    response->http_code = HTTP_CODE_OK;
		break;
	    default:
		response->http_code = HTTP_CODE_NOT_IMPLEMENTED;
		break;
	}
    }

    if (response->http_code != HTTP_CODE_OK)
    {
	if (set_error_page(http_ctx, response))
	{
	    log_message(LOG_LEVEL_ERROR, "set_error_page failed");
	    return -1;
	}
    }

    return 0;
}

#define CHECK(expr) if ((expr) == -1) { goto Exit; }
#define HTTP_INTERNAL_ERROR_MSG "HTTP/1.1 500 Internal Error" HTTP_LINE_END \
    HTTP_LINE_END
#define MAX_BUFSIZE_STR 16
static int respond(http_ctx_t *http_ctx, void *net_ctx,
    http_response_t *response, http_request_t *request)
{
    int rv = -1, response_header_len, fd;
    char *response_header = NULL, *keep_alive_header = NULL,
	buflen_str[MAX_BUFSIZE_STR];

    if ((fd = open(response->path, O_RDONLY)) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "respond, open");
	goto Exit;
    }

    CHECK(response_header_len = http_add_status_line(&response_header,
	response));

    if (request->is_keep_alive)
    {
	CHECK(response_header_len = http_add_header(&response_header,
	    HTTP_HDR_CONNECTION, "keep-alive"));

	if (request->timeout && request->max)
	{
	    snprintf_with_alloc(&keep_alive_header, "timeout=%d max=%d",
		request->timeout, request->max);

	    CHECK(response_header_len = http_add_header(&response_header,
		HTTP_HDR_KEEPALIVE, keep_alive_header));

	    free(keep_alive_header);
	}
    }

    if (!http_ctx->chunked)
    {
	itoa(response->file_size, buflen_str);
	CHECK(response_header_len = http_add_header(&response_header,
	    HTTP_HDR_CONTENT_LENGTH, buflen_str));
    }

    if (http_ctx->chunked)
    {
	CHECK(response_header_len = http_add_header(&response_header,
	    HTTP_HDR_TRANSFER_ENCODING, "chunked"));
    }

    CHECK(response_header_len = http_add_header_end(&response_header));

    if (http_ctx->send(net_ctx, response_header, response_header_len) == -1)
    {
	log_message(LOG_LEVEL_ERROR, "http_ctx->send(header)");
	goto Exit;
    }

    if (http_ctx->chunked)
    {
	if (send_chunked(http_ctx, net_ctx, fd))
	    goto Exit;
    }
    else
    {
	if (send_not_chunked(http_ctx, net_ctx, fd))
	    goto Exit;
    }

    rv = 0;

Exit:

    if (rv)
	http_ctx->send(net_ctx, HTTP_INTERNAL_ERROR_MSG,
	    strlen(HTTP_INTERNAL_ERROR_MSG));

    free(response_header);
    if (fd != -1)
	close(fd);
    return rv;
}

/* Format timeout=%d, max=%d */
typedef struct
{
    char *keyword;
    int *value;
} key_value_t;

static int handle_keep_alive_header(http_request_t *req, char *value, int len)
{
    int i = 0;
    char *delim, *delim2, *value_end = NULL;
    key_value_t key_values[2] = {
	{ .keyword = "timeout", .value = &req->timeout },
	{ .keyword = "max", .value = &req->max }
    };

    if (!req->is_keep_alive)
	goto BadRequest;

    delim = strtok(value, ",");

    if (!delim)
	goto BadRequest;

    while(delim)
    {
	if (i > 1)
	    goto BadRequest;

	/* Skip OWS */
	for (int j = 0; delim + 1; ++j)
	{
	    if (isalpha(*(delim + j)))
		break;
	    else
		delim++;
	}

	delim2 = strchr(delim, '=');
	if (!delim2)
	    goto BadRequest;

	if (!strncmp(delim, key_values[i].keyword, delim2 - delim))
	{
	    delim2++;
	    *(key_values[i].value) = (int)strtol(delim2, &value_end, 10);

	    if (value_end == delim2)
		goto BadRequest;
	}
	else
	{
	    goto BadRequest;
	}

	delim = strtok(NULL, ",");
	++i;
    }

    return 0;

BadRequest:
    log_message(LOG_LEVEL_DEBUG, "parsing Keep-Alive header: invalid value");
    return -1;
}

static int handle_connection_header(http_request_t *req, char *value, int len)
{
    for (int i = 0; i < len; ++i)
    {
	value[i] = tolower(value[i]);

	if (!isalpha(value[i]) && value[i] != '-')
	{
	    len = i;
	    break;
	}
    }

    if (!strncmp(value, "keep-alive", len))
	req->is_keep_alive = 1;
    else if (!strncmp(value, "close", len))
	req->is_keep_alive = 0;
    else
	goto BadRequest;

    return 0;

BadRequest:
    log_message(LOG_LEVEL_DEBUG, "parsing Connection header: invalid value");
    return -1;
}

static int register_header_handler(char *header, hdr_handler_t handler,
    http_ctx_t *http_ctx)
{
    if (!(http_ctx->hdr_handlers[http_ctx->hdr_counter] =
	calloc(1, sizeof(hdr_handler_ctx_t))))
    {
	log_message(LOG_LEVEL_ERROR, "hdr_handler allocation failed");
	return -1;
    }

    http_ctx->hdr_handlers[http_ctx->hdr_counter]->header = header;
    http_ctx->hdr_handlers[http_ctx->hdr_counter]->handler = handler;
    http_ctx->hdr_counter++;

    return 0;
}

http_ctx_t* http_init()
{
    http_ctx_t *http_ctx = malloc(sizeof(*http_ctx));

    if (!http_ctx)
	log_message(LOG_LEVEL_ERROR, "http_ctx memory allocation");

    if (register_header_handler("Connection", handle_connection_header,
	http_ctx))
    {
	log_message(LOG_LEVEL_ERROR, "register Connection header failed");
    }

    if (register_header_handler("Keep-Alive", handle_keep_alive_header,
	http_ctx))
    {
	log_message(LOG_LEVEL_ERROR, "register Keep-Alive header failed");
    }

    return http_ctx;
}

void http_deinit(http_ctx_t *http_ctx)
{
    for (int i = 0; i < http_ctx->hdr_counter; ++i)
	free(http_ctx->hdr_handlers[i]);

    free(http_ctx);
}

void http_set_root_folder(http_ctx_t *http_ctx, char *path)
{
    http_ctx->root_folder = path;
}

void http_set_chunked(http_ctx_t *http_ctx, int is_chunked)
{
    http_ctx->chunked = is_chunked;
}

void http_set_callback(http_ctx_t *http_ctx, http_cb_t http_cb, void *cb)
{
    switch(http_cb)
    {
	case HTTP_CB_RECV:
	    http_ctx->recv = cb;
	    break;
	case HTTP_CB_SET_RECV_TIMEOUT:
	    http_ctx->set_recv_timeout = cb;
	    break;
	case HTTP_CB_SEND:
	    http_ctx->send = cb;
	    break;
	default:
	    break;
    }
}

int http_handle_peer(http_ctx_t *http_ctx, char client_address[],void *net_ctx)
{
    char buffer[BUFSIZE] = {};
    http_request_t request = {};
    http_response_t response = {};
    request.is_keep_alive = 1;
    int buffer_len = 0, request_counter = 0, rv = -1;

    while(request.is_keep_alive)
    {
	request_counter++;

	if (request.timeout)
	{
	    if ((http_ctx->set_recv_timeout(net_ctx, request.timeout) == -1))
	    {
		log_message(LOG_LEVEL_ERROR, "Failed to set recv timeout");
		goto Exit;
	    }
	}

	/* TODO Handle case when request larger than BUFSIZE */
	if ((buffer_len = http_ctx->recv(net_ctx, buffer, BUFSIZE)) == -1)
	{
	    if (errno == EAGAIN || errno == EWOULDBLOCK)
	    {
		log_message(LOG_LEVEL_DEBUG, "Timeout on recv");
		return 0;
	    }

	    log_message(LOG_LEVEL_ERROR, "http_ctx->recv");
	    goto Exit;
	}

	if (!buffer_len)
	{
	    log_message(LOG_LEVEL_DEBUG, "client closed connection");
	    return 0;
	}

	log_message(LOG_LEVEL_DEBUG, "received request");

	if (parse_request(buffer, buffer_len, &request, http_ctx))
	{
	    response.http_code = HTTP_CODE_BAD_REQUEST;
	    request.is_keep_alive = 0;
	}

	if (create_response(http_ctx, &request, &response))
	{
	    log_message(LOG_LEVEL_ERROR, "create_response");
	    goto Exit;
	}

	if (respond(http_ctx, net_ctx, &response, &request))
	{
	    log_message(LOG_LEVEL_ERROR, "respond");
	    goto Exit;
	}

	if (w3c_log_message(4, client_address,
	    http_method_code2str(request.method), request.file ?: "",
	    http_code2str(response.http_code)))
	{
	    log_message(LOG_LEVEL_WARNING, "w3c_logging");
	}

	log_message(LOG_LEVEL_DEBUG, "responded");

	if (request.max && request_counter >= request.max)
	    request.is_keep_alive = 0;

	rv = 0;
    }

Exit:
    free(request.file);
    free(response.path);
    return rv;
}
