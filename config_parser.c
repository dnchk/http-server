#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config_parser.h"
#include "logger.h"

config_parser_t* config_parser_init(char *path)
{
    config_parser_t *config_parser;

    if (!(config_parser = calloc(1, sizeof(config_parser_t))))
    {
	log_message(LOG_LEVEL_ERROR, "config_parser allocation");
	goto Error;
    }
    
    if (!(config_parser->fp = fopen(path, "r")))
    {
	log_message(LOG_LEVEL_ERROR, "config_parser: fopen\n");
	goto Error;
    }

    return config_parser;

Error:
    free(config_parser);

    return NULL;
}

void config_parser_deinit(config_parser_t *config_parser)
{
    if (config_parser->fp)
	fclose(config_parser->fp);
    free(config_parser);
}

int config_add_keyword(config_parser_t *parser, char *keyword, char *value,
    int value_maxlen)
{
    int n = parser->keywords_counter;

    if (parser->keywords_counter >= MAX_KEYWORDS)
    {
	log_message(LOG_LEVEL_ERROR,
	    "[ERROR] parser->keywords_counter overflow");
	return -1;
    }

    if (!(parser->key_values[n] = malloc(sizeof(key_value_t))))
    {
	log_message(LOG_LEVEL_ERROR, "[ERROR] parser->key_values allocation");
	return -1;
    }

    parser->key_values[n]->keyword = keyword;
    parser->key_values[n]->value = value;
    parser->key_values[n]->value_maxlen = value_maxlen;
    parser->keywords_counter++;

    return 0;
}

static int is_keyword_valid(char *str, int len)
{
    for (int i = 0; i < len; ++i)
    {
	if (!(isalpha(str[i]) || isdigit(str[i]) || str[i] == '_'))
	    return 0;
    }

    return 1;
}

int config_parser_start(config_parser_t *parser)
{
    char *delim_ptr, *buffer = NULL;
    size_t buflen = 0;
    int read;

    /* Format: "<Keyword>":"<Value>", no whitespaces allowed */
    while((read = getline(&buffer, &buflen, parser->fp)) != -1)
    {
	/* Find keyword */
	delim_ptr = strchr(buffer, '"');
	buffer = delim_ptr + 1;
	delim_ptr = strchr(buffer, '"');

	if (!is_keyword_valid(buffer, delim_ptr - buffer))
	{
	    log_message(LOG_LEVEL_ERROR, "config: invalid key");
	    goto Error;
	}

	if (!((*(delim_ptr + 1) == ':') && (*(delim_ptr + 2) == '"')))
	{
	    log_message(LOG_LEVEL_ERROR, "config: invalid format");
	    goto Error;
	}

	for (int i = 0; i < parser->keywords_counter; ++i)
	{
	    if (strncmp(buffer, parser->key_values[i]->keyword,
		delim_ptr - buffer))
	    {
		continue;
	    }

	    parser->key_values[i]->found = 1;

	    buffer = delim_ptr + 3;
	    delim_ptr = strchr(buffer, '"');

	    if (!delim_ptr || !(delim_ptr - buffer))
	    {
		log_message(LOG_LEVEL_ERROR, "config: invalid format");
		goto Error;
	    }

	    if ((delim_ptr - buffer) > parser->key_values[i]->value_maxlen)
	    {
		log_message(LOG_LEVEL_ERROR, "config: value overflow");
		goto Error;
	    }

	    strncpy(parser->key_values[i]->value, buffer, delim_ptr - buffer);
	    break;
	}

    }

    return 0;

Error:
    return -1;
}

int check_all_found(config_parser_t *parser)
{
    for (int i = 0; i < parser->keywords_counter; ++i)
	if (!parser->key_values[i]->found)
	    return -1;
    return 0;
}
