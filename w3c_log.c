#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include "w3c_log.h"
#include "logger.h"

#define MAX_FIELD_STR 128
#define MAX_TIME_STR 96
#define MAX_MESSAGE_STR 1024
#define MAX_ENTRY_STR 128

#define DIR_VER "#Version: 1.0"
#define DIR_DATE "#Date:"
#define DIR_FIELDS "#Fields:"

static char* field_identifier2str(w3c_log_field_t field)
{
    switch(field)
    {
	case W3C_LOG_FIELD_CS_METHOD:
	    return "cs-method";
	case W3C_LOG_FIELD_CS_URI:
	    return "cs-uri";
	case W3C_LOG_FIELD_C_IP:
	    return "c-ip";
	case W3C_LOG_FIELD_SC_STATUS:
	    return "sc-status";
	default:
	    break;
    }

    return "";
}

typedef struct {
    FILE *fp;
} w3c_logger_t;

static w3c_logger_t logger;

static char* get_time_str(int date)
{
    static char time_str[MAX_TIME_STR];
    char *format = date ? "%d-%m-%Y %H:%M:%S" : "%H:%M:%S";
    struct tm *current_time;
    time_t timer;

    time(&timer);
    current_time = localtime(&timer);

    strftime(time_str, MAX_TIME_STR, format, current_time);

    return time_str;
}

#define W3C_FILE_HEADER_FMT "%s\n%s %s\n%s %s\n"
int w3c_log_init(char *logpath, w3c_log_field_t fields[], int fields_num)
{
    char fields_str[MAX_FIELD_STR] = {};

    if (!(logger.fp = fopen(logpath, "w")))
    {
	log_message(LOG_LEVEL_ERROR, "server_log_start fopen");
	return -1;
    }

    strcat(fields_str, "time ");

    for (int i = 0; i < fields_num; ++i)
    {
	strcat(fields_str, field_identifier2str(fields[i]));
	strcat(fields_str, i == fields_num - 1 ? "" : " ");
    }

    if (fprintf(logger.fp, W3C_FILE_HEADER_FMT, DIR_VER, DIR_DATE,
	get_time_str(1), DIR_FIELDS, fields_str) < 0)
    {
	log_message(LOG_LEVEL_ERROR, "server_log writing");
	goto Error;
    }

    return 0;

Error:
    fclose(logger.fp);
    return -1;
}

int w3c_log_message(int n, ...)
{
    char message[MAX_MESSAGE_STR] = {}, *entry;
    va_list args;

    va_start(args, n);

    for (int i = 0; i < n; ++i)
    {
	entry = va_arg(args, char*);

	if (!strcmp(entry, ""))
	    strcat(message, "-");
	else
	    strcat(message, entry);

	strcat(message, (i != (n - 1) ? " " : ""));
    }

    va_end(args);

    if (fprintf(logger.fp, "%s %s\n", get_time_str(0), message) < 0)
    {
	log_message(LOG_LEVEL_ERROR, "server_log writing");

	return -1;
    }

    return 0;
}

void w3c_log_deinit()
{
    if (logger.fp)
	fclose(logger.fp);
}

