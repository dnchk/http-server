#ifndef _CONFIG_PARSER_H_
#define _CONFIG_PARSER_H_

#include <stdio.h>

#define MAX_KEYWORDS 8

typedef struct {
    char *keyword;
    char *value;
    int value_maxlen;
    int found;
} key_value_t;

typedef struct {
    FILE *fp;
    key_value_t *key_values[MAX_KEYWORDS];
    int keywords_counter;
} config_parser_t;

config_parser_t* config_parser_init(char *path);
void config_parser_deinit(config_parser_t *config_parser);
int config_add_keyword(config_parser_t *parser, char *keyword, char *value,
    int value_maxlen);
int config_parser_start(config_parser_t *config_parser);
int check_all_found(config_parser_t *parser);

#endif
