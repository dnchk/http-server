#ifndef _SERVER_LOG_H_
#define _SERVER_LOG_H_

typedef enum {
    W3C_LOG_FIELD_CS_METHOD = 0,
    W3C_LOG_FIELD_CS_URI = 1,
    W3C_LOG_FIELD_C_IP = 2,
    W3C_LOG_FIELD_SC_STATUS = 3
} w3c_log_field_t;

int w3c_log_init(char *log_path, w3c_log_field_t fields[], int fields_num);
void w3c_log_deinit();
int w3c_log_message(int n, ...);

#endif
