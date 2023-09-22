#pragma once

#include <stdint.h>

typedef enum
{
	UNIT_SCOPE_USER = 0,
	UNIT_SCOPE_SYSTEM = 1
} unit_service_scope_t;

typedef struct
{
    char *p_name;
    int prio;
    char *p_sdunit;
    unit_service_scope_t sdscope;
} unit_service_t;

typedef struct
{
    int size;
    unit_service_t *elem;
} unit_services_t;

typedef struct
{
    char *p_unit_name;
    char *p_description;
    unit_services_t services;
} unit_t;

typedef enum
{
    UNIT_PARSE_OKAY = 0,
    UNIT_PARSE_ERR = 1,
    UNIT_PARSE_FILE_ERR = 2,
    UNIT_PARSE_SYN_ERR = 3,
    UNIT_PARSE_BAD_SERV_SCOPE = 4
} unit_parse_result_t;

typedef enum
{
    UNIT_FIND_SUCCESS = 0,
    UNIT_FIND_AMBIGOUS = 1,
    UNIT_FIND_NOTFOUND = 2
} unit_find_result_t;

unit_find_result_t unit_find(const uint16_t id, const char *const p_path, char *p_name);
unit_parse_result_t unit_parse(unit_t **pp_unit, const char *const p_path);
void unit_destroy(unit_t *p_unit);
void unit_activate(unit_t *p_unit);
void unit_deactive(unit_t *p_unit);
