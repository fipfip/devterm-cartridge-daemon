#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "detection.h"
#include "log.h"
#include "notify.h"
#include "pinconfig.h"
#include "unit.h"

#define CONFIG_FILE "/etc/cartridged/config.ini"
#define DEFAULT_CARTDB_PATH "/etc/cartridged/cartdb/"
#define DEFAULT_NOTIFY true

static config_t config = {0};
static unit_t *p_unit_active = NULL;

static void cart_event(const detection_event_t event, const unsigned cart_id);
static void cart_unit_load(const char *const p_unit_path);
static void notify_plugin(unit_t *p_unit);
static void notify_notfound(unsigned int number);

const detection_config_t s_detcfg = {
    .pin_route_en = PIN_ROUTE_EN, .pin_clock = PIN_GPIO_Y0, .pin_data = PIN_GPIO_Y1, .p_event_listener = cart_event};

static void destroy()
{
    detection_deinit();
}

static void setup()
{
    int rc = 0;
    atexit(destroy);
    // Initialize detection module
    detection_init(&s_detcfg);
    // read in configuration
    LOG_INF("Reading configuration from '%s'", CONFIG_FILE);
    rc = config_load(CONFIG_FILE, &config);
    if (rc != 0)
    {
        LOG_WRN("Failed to read configuration at '%s', using fallback values", CONFIG_FILE);
        strncpy(config.cartdb_path, DEFAULT_CARTDB_PATH, sizeof(config.cartdb_path));
        config.notification_enabled = DEFAULT_NOTIFY;
    }
    else
    {
        LOG_INF("Configuration:\ndb_path=%s\nnotifications=%s", config.cartdb_path,
                config.notification_enabled ? "yes" : "no");
    }
}

void loop()
{
    detection_state_t state = detection_handle();
    usleep((state == DETECTION_STATE_WAIT) ? 1000U : 100U);
}

static void cart_event(const detection_event_t event, const unsigned cart_id)
{
    char unit_file[256] = {0};
    unit_find_result_t ufind_res = UNIT_FIND_NOTFOUND;

    switch (event)
    {
    case DETECTION_EVENT_INSERTED:
        LOG_INF("Cartridge inserted! (id=%u)", cart_id);
        ufind_res = unit_find(cart_id, config.cartdb_path, unit_file);
        switch (ufind_res)
        {
        case UNIT_FIND_SUCCESS:
            cart_unit_load(unit_file);
            break;
        case UNIT_FIND_AMBIGOUS:
            LOG_ERR("Cartridge #%04X ambigous unit files", cart_id);
            break;
        case UNIT_FIND_NOTFOUND:
            LOG_ERR("Cartridge #%04X no unit file found", cart_id);
            notify_notfound(cart_id);
            break;
        }
        break;

    case DETECTION_EVENT_REMOVED:
        LOG_INF("Cartridge removed! (id=%u)", cart_id);
        if (p_unit_active)
        {
            unit_deactive(p_unit_active);
            unit_destroy(p_unit_active);
            p_unit_active = NULL;
        }
        break;

    default:
        LOG_FTL("Invalid detection event (%d)", event);
        break;
    }
}

static void unit_print(unit_t *p_unit)
{
    printf("Unit '%s'\n", p_unit->p_unit_name);
    printf("Description: %s\n", p_unit->p_description);
    for (size_t i = 0; i < p_unit->services.size; ++i)
    {
        printf(" - Service '%s'\n", p_unit->services.elem[i].p_name);
        printf("   * Unit='%s'\n", p_unit->services.elem[i].p_sdunit);
        printf("   * Scope=%d\n", p_unit->services.elem[i].sdscope);
    }
}

static void notify_plugin(unit_t *p_unit)
{
    char msg[255] = {0};

    if (!config.notification_enabled)
        return;

    sprintf(msg, "Inserted '%s' cartridge", p_unit->p_unit_name);
    notify_send_to_all("DevTerm Cartridge", msg, NULL);
}

static void notify_notfound(unsigned int number)
{
    char msg[255] = {0};

    if (!config.notification_enabled)
        return;

    sprintf(msg,
            "Could not find description for cartridge no. '%X'.\n"
            "Try to reseat cartridge if software is installed.",
            number);
    notify_send_to_all("DevTerm Cartridge", msg, NULL);
}

static void cart_unit_load(const char *const p_unit_path)
{
    unit_t *p_unit = NULL;
    unit_parse_result_t unit_parse_rc = UNIT_PARSE_ERR;

    LOG_INF("Loading unit file '%s'", p_unit_path);
    unit_parse_rc = unit_parse(&p_unit, p_unit_path);
    if (unit_parse_rc == UNIT_PARSE_OKAY)
    {
        unit_print(p_unit);

        p_unit_active = p_unit;

        notify_plugin(p_unit_active);
        unit_activate(p_unit_active);
    }
    else
    {
        p_unit_active = NULL;
    }
}

int main()
{
    setup();
    LOG_INF("%s", "ExtCart daemon started.");
    /*debug();*/
    while (1)
    {
        loop();
    }
}
