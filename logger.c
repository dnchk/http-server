#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include "logger.h"

typedef struct {
    FILE *fp;
    log_level_t log_level;
} logger_t;

static logger_t logger;

static char* log_level2str(log_level_t log_level)
{
    switch(log_level)
    {
	case LOG_LEVEL_DEBUG:
	    return "DEBUG";
	case LOG_LEVEL_WARNING:
	    return "WARNING";
	case LOG_LEVEL_ERROR:
	    return "ERROR";
	default:
	    break;
    }

    return NULL;
}

void log_init(FILE *stream, log_level_t log_level)
{
    logger.fp = stream;
    logger.log_level = log_level;
}

void log_deinit()
{
    fclose(logger.fp);
}

void log_message(log_level_t log_level, char *format, ...)
{
    va_list args;
    struct tm *current_time;
    time_t timer;

    if (logger.log_level > log_level)
	return;

    time(&timer);
    current_time = localtime(&timer);

    fprintf(logger.fp, "[%s][%02i:%02i:%02i] ", log_level2str(log_level),
	current_time->tm_hour, current_time->tm_min, current_time->tm_sec);

    va_start(args, format);
    vfprintf(logger.fp, format, args);
    va_end(args);

    fprintf(logger.fp, "\n");
}

