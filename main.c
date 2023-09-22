#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "detection.h"
#include "log.h"
#include "pinconfig.h"

static void cart_event(const detection_event_t event, const unsigned cart_id);

const detection_config_t s_detcfg = {
    .pin_route_en = PIN_ROUTE_EN, .pin_clock = PIN_GPIO_Y0, .pin_data = PIN_GPIO_Y1, .p_event_listener = cart_event};

static void destroy()
{
    detection_deinit();
}

static void setup()
{
    atexit(destroy);
    // Initialize detection module
    detection_init(&s_detcfg);
}

void loop()
{
    detection_state_t state = detection_handle();
    usleep((state == DETECTION_STATE_WAIT) ? 1000U : 100U);
}

static void cart_event(const detection_event_t event, const unsigned cart_id)
{
    switch (event)
    {
    case DETECTION_EVENT_INSERTED:
        LOG_INF("Cartridge inserted! (id=%u)", cart_id);
        break;

    case DETECTION_EVENT_REMOVED:
        LOG_INF("Cartridge removed! (id=%u)", cart_id);
        break;

    default:
        LOG_FTL("Invalid detection event (%d)", event);
        break;
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
