
#include "unit.h"
#include "log.h"
#include "util.h"

#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <mini.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>

#define LEX(str, fun)                                                                                                  \
    (unit_lex_t)                                                                                                       \
    {                                                                                                                  \
        .p_lex = str, .length = sizeof(str), .p_fun = (lex_parse_func)fun                                              \
    }

typedef unit_parse_result_t (*lex_parse_func)(void * /*p_ctx*/, char * /*p_value*/);

typedef struct
{
    const char *const p_lex;
    size_t length;
    lex_parse_func p_fun;
} unit_lex_t;

static unit_parse_result_t parse_name(unit_t *p_unit, char *p_value);
static unit_parse_result_t parse_desc(unit_t *p_unit, char *p_value);

unit_lex_t KEYS_CARTRIDGE[] = {
    LEX("Name", parse_name),
    LEX("Description", parse_desc),
};

static unit_parse_result_t parse_service_scope(unit_service_t *p_serv, char *p_value);
static unit_parse_result_t parse_service_unit(unit_service_t *p_serv, char *p_value);

unit_lex_t KEYS_SERVICE[] = {
    LEX("Scope", parse_service_scope),
    LEX("Unit", parse_service_unit),
};

static int unit_systemd_servcall(const char *const p_method, const char *const p_service);

unit_find_result_t unit_find(const uint16_t id, const char *const p_path, char *p_name)
{
    unit_find_result_t ret = UNIT_FIND_SUCCESS;
    glob_t result;
    char globpath[255] = {0};

    ///@todo evaluate return code
    (void)snprintf(globpath, sizeof(globpath), "%s/%04X-*.cart", p_path, id);
    const int rc = glob(globpath, 0, NULL, &result);
    if ((rc == 0) && (result.gl_pathc == 1))
    {
        strcpy(p_name, result.gl_pathv[0]);
        ret = UNIT_FIND_SUCCESS;
    }
    else if (rc != 0)
    {
        ret = UNIT_FIND_NOTFOUND;
    }
    else
    {
        ret = UNIT_FIND_AMBIGOUS;
    }
    globfree(&result);
    return ret;
}

unit_parse_result_t unit_parse_cartridge(mini_t *p_mini, unit_t *p_unit)
{
    unit_parse_result_t rc = UNIT_PARSE_OKAY;
    int i = 0;

    mini_next(p_mini);
    for (i = 0; (i < NELEMS(KEYS_CARTRIDGE)) && (rc == UNIT_PARSE_OKAY); ++i)
    {
        if (!p_mini->key)
        {
            rc = UNIT_PARSE_SYN_ERR;
        }
        else if (!p_mini->value)
        {
            rc = UNIT_PARSE_SYN_ERR;
        }
        else
        {
            if (strncmp(p_mini->key, KEYS_CARTRIDGE[i].p_lex, KEYS_CARTRIDGE[i].length - 1) == 0)
            {
                rc = KEYS_CARTRIDGE[i].p_fun(p_unit, p_mini->value);
                if (i != (NELEMS(KEYS_CARTRIDGE) - 1))
                {
                    mini_next(p_mini);
                }
            }
        }
    }
    return rc;
}

static unit_parse_result_t parse_name(unit_t *p_unit, char *p_value)
{
    p_unit->p_unit_name = malloc(strlen(p_value));
    if (p_unit->p_unit_name)
    {
        strcpy(p_unit->p_unit_name, p_value);
    }
    return UNIT_PARSE_OKAY;
}

static unit_parse_result_t parse_desc(unit_t *p_unit, char *p_value)
{
    p_unit->p_description = malloc(strlen(p_value));
    if (p_unit->p_description)
    {
        strcpy(p_unit->p_description, p_value);
    }
    return UNIT_PARSE_OKAY;
}

unit_parse_result_t unit_parse_service(mini_t *p_mini, unit_service_t *p_serv, const char *const p_servicename)
{
    unit_parse_result_t rc = UNIT_PARSE_OKAY;
    int i = 0;

    // Copy name
    p_serv->p_name = malloc(strlen(p_servicename));
    if (p_serv->p_name)
    {
        strcpy(p_serv->p_name, p_servicename);
    }

    mini_next(p_mini);
    for (i = 0; (i < NELEMS(KEYS_SERVICE)) && (rc == UNIT_PARSE_OKAY); ++i)
    {
        if (!p_mini->key)
        {
            rc = UNIT_PARSE_SYN_ERR;
        }
        else if (!p_mini->value)
        {
            rc = UNIT_PARSE_SYN_ERR;
        }
        else
        {
            if (strncmp(p_mini->key, KEYS_SERVICE[i].p_lex, KEYS_SERVICE[i].length - 1) == 0)
            {
                rc = KEYS_SERVICE[i].p_fun(p_serv, p_mini->value);
                if (i != (NELEMS(KEYS_SERVICE) - 1))
                {
                    mini_next(p_mini);
                }
            }
        }
    }

    return rc;
}

static unit_parse_result_t parse_service_unit(unit_service_t *p_serv, char *p_value)
{
    p_serv->p_sdunit = malloc(strlen(p_value));
    if (p_serv->p_sdunit)
    {
        strcpy(p_serv->p_sdunit, p_value);
    }
    return UNIT_PARSE_OKAY;
}

static unit_parse_result_t parse_service_scope(unit_service_t *p_serv, char *p_value)
{
    unit_parse_result_t rc = UNIT_PARSE_OKAY;

    if (strncmp(p_value, "System", sizeof("System") - 1) == 0)
    {
        p_serv->sdscope = UNIT_SCOPE_SYSTEM;
    }
    else if (strncmp(p_value, "User", sizeof("User") - 1) == 0)
    {
        p_serv->sdscope = UNIT_SCOPE_USER;
    }
    else
    {
        rc = UNIT_PARSE_BAD_SERV_SCOPE;
    }
    return rc;
}

unit_parse_result_t unit_parse_section(mini_t *p_mini, unit_t *p_unit, const char *const p_section,
                                       unit_service_t *p_services, size_t *p_service_cnt)
{
    unit_parse_result_t rc = UNIT_PARSE_OKAY;
    if (strncmp(p_section, "Cartridge", sizeof("Cartridge") - 1) == 0)
    {
        rc = unit_parse_cartridge(p_mini, p_unit);
    }
    else if (strncmp(p_section, "Service", sizeof("Service") - 1) == 0)
    {
        rc = unit_parse_service(p_mini, &p_services[*p_service_cnt], &p_section[sizeof("Service ") - 1]);
        (*p_service_cnt)++;
    }

    return rc;
}

unit_parse_result_t unit_parse(unit_t **pp_unit, const char *const p_path)
{
    unit_parse_result_t parse_rc = UNIT_PARSE_OKAY;
    mini_t *p_mini = NULL;
    unit_t *p_unit = NULL;
    unit_service_t services[16];
    size_t service_cnt = 0;

    p_mini = mini_init(p_path);
    p_unit = malloc(sizeof(*p_unit));

    while (mini_next(p_mini))
    {
        if (!p_mini->key)
        {
            parse_rc = unit_parse_section(p_mini, p_unit, p_mini->section, services, &service_cnt);
        }
    }
    if (!p_mini->eof)
    {
        LOG_ERR("Error reading configuration at '%s' (error '%s')", p_path, strerror(errno));
        parse_rc = UNIT_PARSE_FILE_ERR;
    }
    mini_free(p_mini);

    if (parse_rc != UNIT_PARSE_OKAY)
    {
        free(p_unit);
    }
    else
    {
        if (service_cnt > 0)
        {
            p_unit->services.size = service_cnt;
            p_unit->services.elem = malloc(sizeof(unit_service_t) * service_cnt);
            memcpy(p_unit->services.elem, services, sizeof(unit_service_t) * service_cnt);
        }

        *pp_unit = p_unit;
    }

    return parse_rc;
}

void unit_destroy(unit_t *p_unit)
{
    size_t i = 0;
    for (i = 0; i < p_unit->services.size; ++i)
    {
        free(p_unit->services.elem[i].p_name);
        free(p_unit->services.elem[i].p_sdunit);
    }
    free(p_unit->services.elem);
    free(p_unit->p_unit_name);
    free(p_unit->p_description);
}

void unit_activate(unit_t *p_unit)
{
    LOG_INF("Starting Cartridge Unit '%s'", p_unit->p_unit_name);
    for (size_t i = 0; i < p_unit->services.size; ++i)
    {
        switch (p_unit->services.elem[i].sdscope)
        {
        case UNIT_SCOPE_SYSTEM:
            // make d-bus call to systemd starting the service
            unit_systemd_servcall("StartUnit", p_unit->services.elem[i].p_sdunit);
            break;
        default:
            LOG_WRN("Unsupported scope=%d for service '%s' of '%s', ignoring.", p_unit->services.elem[i].sdscope,
                    p_unit->services.elem[i].p_name, p_unit->p_unit_name);
        }
    }
}

void unit_deactive(unit_t *p_unit)
{
    LOG_INF("Stopping Cartridge Unit '%s'", p_unit->p_unit_name);
    for (size_t i = 0; i < p_unit->services.size; ++i)
    {
        switch (p_unit->services.elem[i].sdscope)
        {
        case UNIT_SCOPE_SYSTEM:
            // make d-bus call to systemd stopping the service
            unit_systemd_servcall("StopUnit", p_unit->services.elem[i].p_sdunit);
            break;
        default:
            LOG_WRN("Unsupported scope=%d for service '%s' of '%s', ignoring.", p_unit->services.elem[i].sdscope,
                    p_unit->services.elem[i].p_name, p_unit->p_unit_name);
        }
    }
}

static int unit_systemd_servcall(const char *const p_method, const char *const p_service)
{
    char serv_name[255] = {0};
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus *bus = NULL;
    const char *path;
    int rc;

    // Append .service to p_service
    ///@todo evaluate return code
    (void)snprintf(serv_name, sizeof(serv_name), "%s.service", p_service);

    // Connect to systemd system bus
    rc = sd_bus_open_system(&bus);
    if (rc < 0)
    {
        LOG_ERR("Failed to connect to system bus: %s\n", strerror(-rc));
        goto finish;
    }

    // Issue the method call and store the response message in m
    rc = sd_bus_call_method(bus, "org.freedesktop.systemd1",    /* service to contact */
                            "/org/freedesktop/systemd1",        /* object path */
                            "org.freedesktop.systemd1.Manager", /* interface name */
                            p_method,                           /* method name, e.g "StartUnit" or "StopUnit" */
                            &error,                             /* object to return error in */
                            &m,                                 /* return message on success */
                            "ss",                               /* input signature */
                            serv_name,                          /* first argument */
                            "replace");                         /* second argument */
    if (rc < 0)
    {
        LOG_ERR("Failed to issue method call: %s\n", error.message);
        goto finish;
    }

    // Parse the response message
    rc = sd_bus_message_read(m, "o", &path);
    if (rc < 0)
    {
        LOG_ERR("Failed to parse response message: %s\n", strerror(-rc));
        goto finish;
    }

    LOG_INF("Queued service job as %s.\n", path);

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    sd_bus_unref(bus);

    return rc;
}
