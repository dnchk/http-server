#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdio.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_WARNING = 1,
    LOG_LEVEL_ERROR = 2
} log_level_t;

void log_init(FILE *stream, log_level_t log_level);
void log_deinit();
void log_message(log_level_t log_level, char *message, ...);

#endif
