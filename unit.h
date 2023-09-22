#pragma once

typedef struct
{
    char *p_name;
    int prio;
    char *p_sdunit;
} unit_service_t;

typedef struct
{
    int size;
    cart_service_t *elem;
} unit_services_t;

typedef struct
{
    char *p_unit_name;
    char *p_description;
    cart_services_t services;
} unit_t;

typedef enum
{
    UNIT_PARSE_OKAY = 0
} unit_parse_result_t;

typedef enum
{
    UNIT_FIND_SUCCESS = 0,
    UNIT_FIND_NOTFOUND = 1
} unit_find_result_t;

int unit_find(const uint16_t id, const char *const p_path, char *p_name);
unit_parse_result_t unit_parse(unit_t **pp_unit, const char *const p_path);
