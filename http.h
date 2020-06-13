#ifndef _HTTP_H_
#define _HTTP_H_

#define HANDLERS_MAX 10

typedef enum {
    HTTP_CODE_OK = 200,
    HTTP_CODE_BAD_REQUEST = 400,
    HTTP_CODE_NOT_FOUND = 404,
    HTTP_CODE_NOT_IMPLEMENTED = 501
} http_code_t;

typedef enum {
    HTTP_CB_RECV = 0,
    HTTP_CB_SET_RECV_TIMEOUT = 1,
    HTTP_CB_SEND = 2
} http_cb_t;

typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_HEAD = 1,
    HTTP_METHOD_POST = 2,
    HTTP_METHOD_PUT = 3,
    HTTP_METHOD_DELETE = 4,
    HTTP_METHOD_CONNECT = 5,
    HTTP_METHOD_OPTIONS = 6,
    HTTP_METHOD_TRACE = 7,
    HTTP_METHOD_PATCH = 8,
    HTTP_METHOD_UNKNOWN = 9
} http_method_t;

typedef int (*http_recv_t)(void* net_ctx, char *buffer, int buffer_len);
typedef int (*http_set_recv_timeout_t)(void* net_ctx, int timeout);
typedef int (*http_send_t)(void* net_ctx, char *buffer, int buffer_len);

typedef struct {
    char *file;
    http_method_t method;
    int is_keep_alive;
    int timeout;
    int max;
} http_request_t;

typedef int (*hdr_handler_t)(http_request_t *req, char *val, int len);

typedef struct {
    char *header;
    hdr_handler_t handler;
} hdr_handler_ctx_t;

typedef struct {
    http_recv_t recv;
    http_set_recv_timeout_t set_recv_timeout;
    http_send_t send;
    char *root_folder;
    int chunked;
    hdr_handler_ctx_t *hdr_handlers[HANDLERS_MAX];
    int hdr_counter;
} http_ctx_t;

http_ctx_t* http_init();
void http_deinit(http_ctx_t *http_ctx);
void http_set_root_folder(http_ctx_t *http_ctx, char *path);
void http_set_chunked(http_ctx_t *http_ctx, int is_chunked);
void http_set_callback(http_ctx_t *http_ctx, http_cb_t http_cb, void *cb);
int http_handle_peer(http_ctx_t *http_ctx, char client_address[],void *net_ctx);

#endif
