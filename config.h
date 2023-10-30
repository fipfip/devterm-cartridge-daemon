#pragma once

#include <stdbool.h>

typedef struct
{
    char cartdb_path[255];
    bool notification_enabled;
} config_t;

int config_load(const char *const p_filename, config_t *p_config);
