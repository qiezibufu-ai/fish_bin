#include "app_dosing_control.h"

#include "adc_port.h"
#include "dosing_control_config.h"
#include "osal_task.h"
#include "panel_port.h"
#include "service_bubble.h"

#include <inttypes.h>
#include <stdio.h>

typedef enum
{
    CONTROL_UNCALIBRATED,
    CONTROL_CALIBRATING,
    CONTROL_CAL_FAILED,
    CONTROL_READY,
    CONTROL_DOSING,
    CONTROL_DOSE_FAILED
} control_state_t;

typedef struct
{
    uint32_t value;
    bool digits;
    bool discard;
    bool armed;
} control_rx_t;

typedef struct
{
    service_dosing_t *dosing;
    service_bubble_t bubble;
    fw_error_record_t error;
    control_state_t state;
    uint32_t entered_ms;
    uint32_t last_sample_ms;
    uint32_t button_ms;
    uint32_t led_key;
    control_rx_t rx[CORE_PANEL_RX_COUNT];
    bool button_raw;
    bool button_stable;
    bool button_armed;
    bool liquid_valid;
    bool liquid_present;
} control_t;

static control_t s_control;

_Static_assert(DOSING_CONTROL_TIMEOUT_MS > 0U && DOSING_CONTROL_TIMEOUT_MS <= 60000U,
               "invalid dosing timeout");
_Static_assert(DOSING_CONTROL_LED_LEVEL <= 255U, "invalid WS2812 brightness");

static void change_state(control_state_t state, uint32_t now)
{
    s_control.state = state;
    s_control.entered_ms = now;
    for (unsigned i = 0; i < CORE_PANEL_RX_COUNT; ++i)
    {
        s_control.rx[i].armed = false;
        s_control.rx[i].discard |= s_control.rx[i].digits;
    }
}

static void control_fail_at(control_state_t state, fw_error_t code, fw_error_t cause,
                            const fw_error_record_t *origin, uint32_t now, const char *file,
                            const char *function, uint32_t line)
{
    fw_error_capture_at(&s_control.error, code, cause, (uint32_t)s_control.state,
                        (uint16_t)s_control.state, s_control.dosing->result.request_id,
                        now, file, function, line);
    if (origin != NULL && origin->code != FW_OK)
    {
        fw_error_set_origin(&s_control.error, origin);
        s_control.error.cause_code = cause;
        s_control.error.cleanup_code = origin->cleanup_code;
    }
    change_state(state, now);
}

#define CONTROL_FAIL(state, code, cause, origin, now) \
    control_fail_at((state), (code), (cause), (origin), (now), __FILE__, __func__, __LINE__)

static bool boot_clicked(uint32_t now)
{
    bool pressed = core_panel_boot_pressed();
    if (pressed != s_control.button_raw)
    {
        s_control.button_raw = pressed;
        s_control.button_ms = now;
    }
    if (now - s_control.button_ms < DOSING_CONTROL_BUTTON_MS)
    {
        return false;
    }
    if (!pressed)
    {
        s_control.button_armed = true;
    }
    if (pressed == s_control.button_stable)
    {
        return false;
    }
    s_control.button_stable = pressed;
    return pressed && s_control.button_armed;
}

static void show_light(uint32_t now)
{
    const uint8_t level = DOSING_CONTROL_LED_LEVEL;
    uint8_t red = 0U, green = 0U, blue = 0U;
    uint32_t elapsed = now - s_control.entered_ms;
    switch (s_control.state)
    {
        case CONTROL_UNCALIBRATED: red = level; green = level; break;
        case CONTROL_CALIBRATING: green = elapsed % 250U < 125U ? level : 0U; break;
        case CONTROL_CAL_FAILED: red = elapsed % 1000U < 500U ? level : 0U; break;
        case CONTROL_READY: green = level; break;
        case CONTROL_DOSING: blue = elapsed % 500U < 250U ? level : 0U; break;
        case CONTROL_DOSE_FAILED: red = level; break;
        default: red = level; break;
    }
    uint32_t key = ((uint32_t)red << 16U) | ((uint32_t)green << 8U) | blue;
    if (key != s_control.led_key && core_panel_rgb(red, green, blue) == FW_OK)
    {
        s_control.led_key = key;
    }
}

static void calibration_step(uint32_t now)
{
    core_adc_sample_t sample;
    fw_error_record_t error;
    fw_error_t err = core_adc_read(&sample, &error);
    if (err == FW_OK)
    {
        err = sample.voltage_valid ? service_bubble_add(&s_control.bubble,
                                                        (uint32_t)sample.voltage_mv)
                                   : FW_ERR_PORT_NOT_SUPPORTED;
    }
    if (err != FW_OK)
    {
        CONTROL_FAIL(CONTROL_CAL_FAILED, FW_ERR_APP_CALIBRATION, err, &error, now);
        printf("calibration: failed cause=%s min_mv=%" PRIu32 " max_mv=%" PRIu32 "\n",
               fw_error_code_name(err), s_control.bubble.samples ? s_control.bubble.min_mv : 0U,
               s_control.bubble.max_mv);
    }
    else if (s_control.bubble.valid)
    {
        change_state(CONTROL_READY, now);
        printf("calibration: ready baseline_mv=%" PRIu32 " spread_mv=%" PRIu32 "\n",
               s_control.bubble.baseline_mv, s_control.bubble.max_mv - s_control.bubble.min_mv);
    }
}

static void submit(uint32_t value, uint32_t now, const char *source)
{
    const service_dosing_request_t request = {
        .target_drops = value, .speed_centi_rpm = DOSING_CONTROL_PUMP_SPEED_CENTI_RPM,
        .reverse = DOSING_CONTROL_REVERSE != 0,
        .no_drop_ms = DOSING_CONTROL_TIMEOUT_MS, .max_run_ms = DOSING_CONTROL_TIMEOUT_MS,
        .tail_quiet_ms = DOSING_CONTROL_TAIL_QUIET_MS, .tail_max_ms = DOSING_CONTROL_TAIL_MAX_MS
    };
    fw_error_t err = service_dosing_start(s_control.dosing, &request);
    if (err != FW_OK)
    {
        printf("dosing: rejected %s\n", fw_error_code_name(err));
        return;
    }
    s_control.liquid_valid = false;
    s_control.last_sample_ms = now - DOSING_CONTROL_SAMPLE_MS;
    change_state(CONTROL_DOSING, now);
    printf("dosing: accepted target=%" PRIu32 " source=%s\n", value, source);
}

/* 整行验收；各通道独立且不让未标定/忙时的半行带到下一轮。 */
static void read_commands(core_panel_rx_t source, uint32_t now)
{
    control_rx_t *rx = &s_control.rx[source];
    const char *name = source == CORE_PANEL_RX_USB ? "USB" : "UART0";
    char data[32];
    size_t count;
    if (core_panel_read(source, data, sizeof(data), &count) != FW_OK)
    {
        rx->discard |= rx->digits;
        rx->armed = false;
        return;
    }
    if (count == 0U && s_control.state == CONTROL_READY)
    {
        rx->armed = true;
    }
    for (size_t i = 0; i < count; ++i)
    {
        char ch = data[i];
        bool allowed = s_control.state == CONTROL_READY && rx->armed;
        if (ch == '\r' || ch == '\n')
        {
            bool nonempty = rx->digits || rx->discard;
            bool valid = allowed && !rx->discard && rx->digits && rx->value != 0U;
            uint32_t value = rx->value;
            if (allowed && !valid && nonempty)
            {
                printf("dosing: source=%s enter 1..10000 followed by Enter\n", name);
            }
            else if (nonempty && (s_control.state == CONTROL_UNCALIBRATED ||
                                 s_control.state == CONTROL_CAL_FAILED))
            {
                printf("dosing: source=%s rejected; calibrate with BOOT first\n", name);
            }
            rx->value = 0U;
            rx->digits = false;
            rx->discard = false;
            if (valid)
            {
                submit(value, now, name);
            }
        }
        else if (!allowed || ch < '0' || ch > '9')
        {
            rx->discard = true;
        }
        else if (!rx->discard)
        {
            uint32_t next = rx->value * 10U + (uint32_t)(ch - '0');
            rx->digits = true;
            rx->discard = next > 10000U;
            if (!rx->discard)
            {
                rx->value = next;
            }
        }
    }
}

static void dose_step(uint32_t now, bool sample_due)
{
    service_dosing_t *dosing = s_control.dosing;
    if (sample_due && dosing->result.state == SERVICE_DOSING_RUNNING)
    {
        core_adc_sample_t sample;
        fw_error_record_t error;
        s_control.liquid_valid = core_adc_read(&sample, &error) == FW_OK && sample.voltage_valid &&
            service_bubble_liquid(&s_control.bubble, (uint32_t)sample.voltage_mv,
                                  &s_control.liquid_present) == FW_OK;
    }
    (void)service_dosing_step(dosing);
    service_dosing_result_t result;
    if (service_dosing_get_result(dosing, &result) != FW_OK)
    {
        CONTROL_FAIL(CONTROL_DOSE_FAILED, FW_ERR_APP_DOSE_EXIT, FW_ERR_DOSING_NOT_READY, NULL, now);
        return;
    }
    if (result.state == SERVICE_DOSING_DONE)
    {
        change_state(CONTROL_READY, osal_task_now_ms());
        printf("dosing: DONE target=%" PRIu32 " stop=%" PRIu32 " total=%" PRIu32 "\n",
               result.target_drops, result.stop_drops, result.drops);
    }
    else if (result.state == SERVICE_DOSING_FAULT || result.state == SERVICE_DOSING_CANCELLED)
    {
        bool recalibrate = result.error.code == FW_ERR_DOSING_RUN_TIMEOUT &&
                           result.count_valid && result.drops == 0U && result.stop_acked &&
                           s_control.liquid_valid && !s_control.liquid_present;
        if (recalibrate)
        {
            /* 已收到STOP且命令端口健康，保留原绑定供重新标定后使用。 */
            const service_dosing_ops_t ops = dosing->ops;
            service_bubble_reset(&s_control.bubble);
            CONTROL_FAIL(CONTROL_CAL_FAILED, FW_ERR_APP_CALIBRATION, result.error.code,
                         &result.error, osal_task_now_ms());
            (void)service_dosing_init(dosing, &ops);
        }
        else
        {
            CONTROL_FAIL(CONTROL_DOSE_FAILED, FW_ERR_APP_DOSE_EXIT, result.error.code,
                         &result.error, osal_task_now_ms());
        }
        printf("dosing: %s cause=%s drops=%" PRIu32 " stop_acked=%u cleanup=%s\n",
               recalibrate ? "recalibrate: empty and zero drops" : "FAILED",
               fw_error_code_name(result.error.code), result.drops, (unsigned)result.stop_acked,
               fw_error_code_name(result.error.cleanup_code));
    }
}

void app_dosing_control_init(service_dosing_t *dosing)
{
    uint32_t now = osal_task_now_ms();
    s_control = (control_t){.dosing = dosing, .led_key = UINT32_MAX,
                            .entered_ms = now, .button_ms = now};
    puts("mode: dosing_control");
    puts("calibration: press BOOT with the tube empty");
}

void app_dosing_control_step(void)
{
    uint32_t now = osal_task_now_ms();
    bool clicked = boot_clicked(now);
    switch (s_control.state)
    {
        case CONTROL_UNCALIBRATED:
        case CONTROL_CAL_FAILED:
        case CONTROL_READY:
            if (clicked)
            {
                service_bubble_reset(&s_control.bubble);
                s_control.error = (fw_error_record_t){0};
                change_state(CONTROL_CALIBRATING, now);
                s_control.last_sample_ms = now - DOSING_CONTROL_SAMPLE_MS;
            }
            break;
        case CONTROL_CALIBRATING:
        case CONTROL_DOSING:
        case CONTROL_DOSE_FAILED:
            break;
        default:
            (void)service_dosing_cancel(s_control.dosing);
            CONTROL_FAIL(CONTROL_DOSE_FAILED, FW_ERR_APP_CONTROL_STATE, FW_OK, NULL, now);
            break;
    }
    read_commands(CORE_PANEL_RX_UART0, now);
    read_commands(CORE_PANEL_RX_USB, now);
    show_light(now);
    bool sample_due = now - s_control.last_sample_ms >= DOSING_CONTROL_SAMPLE_MS;
    if (sample_due)
    {
        s_control.last_sample_ms = now;
    }
    if (s_control.state == CONTROL_CALIBRATING && sample_due)
    {
        calibration_step(now);
    }
    else if (s_control.state == CONTROL_DOSING)
    {
        dose_step(now, sample_due);
    }
}

fw_error_t app_dosing_control_get_error(fw_error_record_t *error)
{
    if (error == NULL)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    *error = s_control.error;
    return FW_OK;
}
