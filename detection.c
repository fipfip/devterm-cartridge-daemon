#include "detection.h"

#include <errno.h>
#include <gpiod.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "pinconfig.h"

#define ROUTE_EN_ACTIVE (0)
#define ROUTE_EN_INACTIVE (1)
#define CARTID_MAX_BYTES (4)

typedef enum
{
    DETECTION_READSTATE_IDLE,
    DETECTION_READSTATE_START,
    DETECTION_READSTATE_WAITHIGH,
    DETECTION_READSTATE_WAITLOW
} detection_readstate_t;

typedef struct
{
    unsigned chipno;
    struct gpiod_chip *p_chip;
    struct gpiod_line *p_line;
    bool inuse;
} detection_pin_t;

typedef enum
{
    PINIDX_ROUTE_EN = 0,
    PINIDX_CLOCK,
    PINIDX_DATA,
    PINIDX_MAX
} detection_pinidx_t;

static detection_config_t s_config = {
    .pin_route_en = PIN_NONE, .pin_clock = PIN_NONE, .pin_data = PIN_NONE, .p_event_listener = NULL};

static bool s_initialized = false;
static detection_pin_t s_pins[PINIDX_MAX] = {{.chipno = -1, .p_chip = NULL, .p_line = NULL, .inuse = false},
                                             {.chipno = -1, .p_chip = NULL, .p_line = NULL, .inuse = false},
                                             {.chipno = -1, .p_chip = NULL, .p_line = NULL, .inuse = false}};
static detection_state_t s_state = DETECTION_STATE_WAIT;
static detection_readstate_t s_readstate = DETECTION_READSTATE_IDLE;
static unsigned s_pulse_time_start = 0;
static unsigned s_bit_count = 0;
static unsigned s_byte_count = 0;
static unsigned s_cart_id = 0;
static unsigned s_read_byte = 0;

static int hal_init_pin(const detection_pinidx_t idx, detection_pincfg_t *p_pincfg);
static const char *pinidx_to_str(detection_pinidx_t idx);
static void pin_release(const detection_pinidx_t idx);
static void pin_config_input(const detection_pinidx_t idx);
static void pin_config_output(const detection_pinidx_t idx, const int default_val);
static int pin_get(const detection_pinidx_t idx);
static void pin_set(const detection_pinidx_t idx, const int val);

static void config_pins_listening_state(void);
static void config_pins_listening_state(void);
static void set_pins_enable_read(bool enable);
static bool pulse_read(bool *p_bit);
static void handle_wait_for_cart(void);
static void handle_read_cartid(void);
static void handle_inserted(void);

void detection_init(const detection_config_t *const p_cfg)
{
    int rc = 0;
    // copy over configuration
    memcpy(&s_config, p_cfg, sizeof(s_config));

    // initilaize HAL
    rc |= hal_init_pin(PINIDX_ROUTE_EN, &s_config.pin_route_en);
    rc |= hal_init_pin(PINIDX_CLOCK, &s_config.pin_clock);
    rc |= hal_init_pin(PINIDX_DATA, &s_config.pin_data);

    if (rc == 0)
    {
        s_initialized = true;
        config_pins_listening_state();
    }
}

void detection_deinit(void)
{
    // release all lines and gpiochips
    for (int i = 0; i < PINIDX_MAX; ++i)
    {
        pin_release(i);
        gpiod_chip_close(s_pins[i].p_chip);
    }
}

detection_state_t detection_handle()
{
    switch (s_state)
    {
    case DETECTION_STATE_WAIT:
        handle_wait_for_cart();
        break;
    case DETECTION_STATE_READ_ID:
        handle_read_cartid();
        break;
    case DETECTION_STATE_INSERTED:
        handle_inserted();
        break;
    default:
        LOG_FTL("Invalid detection state (%d)", s_state);
        break;
    }

    return s_state;
}

static int hal_init_pin(const detection_pinidx_t idx, detection_pincfg_t *p_pincfg)
{
    // Try to reuse handles if they are already allocated
    for (int i = 0; i < PINIDX_MAX; ++i)
    {
        // Ignore its own index
        if (idx == i)
            continue;

        // This index has its chip initialized already,
        // try to reuse it if the chip number matches
        if (s_pins[i].p_chip && (s_pins[i].chipno == p_pincfg->chip))
        {
            // Reuse the handle
            s_pins[idx].p_chip = s_pins[i].p_chip;
            s_pins[idx].chipno = p_pincfg->chip;
        }
    }

    if (s_pins[idx].chipno == -1)
    {
        // No preexisting allocation for the chip found - make it
        s_pins[idx].p_chip = gpiod_chip_open_by_number(p_pincfg->chip);
        if (!s_pins[idx].p_chip)
        {
            LOG_ERR("Could not allocate gpiochip%d", p_pincfg->chip);
            return -EINVAL;
        }
        else
        {
            s_pins[idx].chipno = p_pincfg->chip;
        }
    }

    // At this point we got the chip, the line is unique for each pin
    /// @todo test for that
    s_pins[idx].p_line = gpiod_chip_get_line(s_pins[idx].p_chip, p_pincfg->line);
    if (!s_pins[idx].p_line)
    {
        LOG_ERR("Could not allocate line %d for gpiochip%d", p_pincfg->line, p_pincfg->chip);
        return -EINVAL;
    }

    return 0;
}

static const char *pinidx_to_str(detection_pinidx_t idx)
{
    const char *p_name = "undefined";
    switch (idx)
    {
    case PINIDX_ROUTE_EN:
        p_name = "route_en";
        break;
    case PINIDX_DATA:
        p_name = "data";
        break;
    case PINIDX_CLOCK:
        p_name = "clock";
        break;
    default:
        break;
    }

    return p_name;
}

static void pin_release(const detection_pinidx_t idx)
{
    if (s_pins[idx].inuse)
        gpiod_line_release(s_pins[idx].p_line);
    s_pins[idx].inuse = false;
}

static void pin_config_input(const detection_pinidx_t idx)
{
    // If the line is already busy, this function releases it
    pin_release(idx);
    const int rc = gpiod_line_request_input(s_pins[idx].p_line, pinidx_to_str(idx));
    if (rc != 0)
    {
        LOG_ERR("Could not request input idx=%d, rc=%d", idx, rc);
    }
    else
    {
        s_pins[idx].inuse = true;
    }
}

static void pin_config_output(const detection_pinidx_t idx, const int default_val)
{
    // If the line is already busy, this function releases it
    pin_release(idx);
    const int rc = gpiod_line_request_output(s_pins[idx].p_line, pinidx_to_str(idx), default_val);
    if (rc != 0)
    {
        LOG_ERR("Could not request output idx=%d, rc=%d", idx, rc);
    }
    else
    {
        s_pins[idx].inuse = true;
    }
}

static int pin_get(const detection_pinidx_t idx)
{
    return gpiod_line_get_value(s_pins[idx].p_line);
}
static void pin_set(const detection_pinidx_t idx, const int val)
{
    gpiod_line_set_value(s_pins[idx].p_line, val);
}

static void config_pins_listening_state(void)
{
    pin_config_input(PINIDX_ROUTE_EN);
    pin_config_input(PINIDX_CLOCK);
    pin_config_input(PINIDX_DATA);
}

static void set_pins_enable_read(bool enable)
{
    pin_set(PINIDX_ROUTE_EN, enable ? 1 : 0);
}

static void config_pins_read_state(void)
{
    pin_config_output(PINIDX_ROUTE_EN, 0);
    pin_config_output(PINIDX_CLOCK, 1);
    pin_config_input(PINIDX_DATA);
}

static void handle_wait_for_cart(void)
{
    int pin_state = pin_get(PINIDX_ROUTE_EN);
    if (pin_state == ROUTE_EN_ACTIVE)
    {
        config_pins_read_state();
        set_pins_enable_read(true);
        s_state = DETECTION_STATE_READ_ID;
    }
}

static bool pulse_read(bool *p_bit)
{
    bool bit_set = false;
    const unsigned instant_now = clock() * 1000000 / CLOCKS_PER_SEC;
    switch (s_readstate)
    {
    case DETECTION_READSTATE_IDLE:
        s_readstate = DETECTION_READSTATE_START;
        pin_set(PINIDX_CLOCK, 1);
    case DETECTION_READSTATE_START:
        pin_set(PINIDX_CLOCK, 1);
        s_pulse_time_start = instant_now;
        s_readstate = DETECTION_READSTATE_WAITHIGH;
        break;
    case DETECTION_READSTATE_WAITHIGH:
        if ((instant_now - s_pulse_time_start) > 100)
        {
            *p_bit = pin_get(PINIDX_DATA);
            bit_set = true;
            pin_set(PINIDX_CLOCK, 0);
            s_pulse_time_start = instant_now;
            s_readstate = DETECTION_READSTATE_WAITLOW;
        }
        break;
    case DETECTION_READSTATE_WAITLOW:
        if ((instant_now - s_pulse_time_start) > 100)
        {
            pin_set(PINIDX_CLOCK, 1);
            s_readstate = DETECTION_READSTATE_IDLE;
        }
        break;
    }
    return bit_set;
}

static void handle_read_cartid(void)
{
    bool bit = false;
    if (pulse_read(&bit))
    {
        // the last bit shifted is the first
        s_read_byte <<= 1;
        s_read_byte |= bit;
        s_bit_count++;
    }
    if (s_bit_count >= 8)
    {
        s_bit_count = 0U;
        s_byte_count++;
        // byte == 0: all bits are drained from 74HC165
        // byte == ff: GPIO_X2 is floating...
        if ((s_read_byte == 0) || (s_read_byte == 0xff))
        {
            s_state = DETECTION_STATE_INSERTED;
        }
        else
        {
            // neither drained nor floating, accept byte
            s_cart_id <<= 8;
            s_cart_id = s_cart_id | s_read_byte;
        }
        s_read_byte = 0;
    }
    if (s_byte_count >= CARTID_MAX_BYTES)
    {
        s_state = DETECTION_STATE_INSERTED;
    }

    if (s_state == DETECTION_STATE_INSERTED)
    {
        s_byte_count = 0U;
        set_pins_enable_read(false);
        config_pins_listening_state();
        // cart insert event
        if (s_config.p_event_listener)
            s_config.p_event_listener(DETECTION_EVENT_INSERTED, s_cart_id);
    }
}

static void handle_inserted(void)
{
    int pin_state = pin_get(PINIDX_ROUTE_EN);
    if (pin_state == ROUTE_EN_INACTIVE)
    {
        // cart eject event
        if (s_config.p_event_listener)
            s_config.p_event_listener(DETECTION_EVENT_REMOVED, s_cart_id);
        s_cart_id = 0U;
        s_state = DETECTION_STATE_WAIT;
    }
}
