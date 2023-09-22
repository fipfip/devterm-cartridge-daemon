#pragma once

typedef enum
{
    DETECTION_EVENT_INSERTED = 10,
    DETECTION_EVENT_REMOVED = 20
} detection_event_t;

typedef enum
{
    DETECTION_STATE_WAIT = 10,
    DETECTION_STATE_READ_ID = 20,
    DETECTION_STATE_INSERTED = 30
} detection_state_t;

typedef void (*detection_event_cb)(const detection_event_t /*event*/, const unsigned /* cart id */);

typedef struct
{
    int chip;
    int line;
} detection_pincfg_t;

typedef struct
{
    detection_pincfg_t pin_route_en;
    detection_pincfg_t pin_clock;
    detection_pincfg_t pin_data;
    detection_event_cb p_event_listener;
} detection_config_t;

void detection_init(const detection_config_t *const p_cfg);
void detection_deinit(void);
detection_state_t detection_handle();
