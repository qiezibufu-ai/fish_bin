/* 运行生产控制应用和服务，用可控时钟、ADC、泵、串口与灯替身验证交互。 */
#include "app_dosing_control.h"
#include "adc_port.h"
#include "dosing_control_config.h"
#include "panel_port.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static service_dosing_t s_dosing;
static uint32_t s_now, s_drops, s_mv, s_led;
static unsigned s_start, s_stop;
static uint32_t s_direction;
static bool s_boot, s_adc_valid;
static const char *s_rx[CORE_PANEL_RX_COUNT];

uint32_t osal_task_now_ms(void)
{
    return s_now;
}

fw_error_t core_adc_read(core_adc_sample_t *sample, fw_error_record_t *error)
{
    *error = (fw_error_record_t){0};
    *sample = (core_adc_sample_t){.voltage_mv = (int)s_mv, .voltage_valid = s_adc_valid};
    return FW_OK;
}

bool core_panel_boot_pressed(void)
{
    return s_boot;
}

fw_error_t core_panel_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    s_led = ((uint32_t)red << 16U) | ((uint32_t)green << 8U) | blue;
    return FW_OK;
}

fw_error_t core_panel_read(core_panel_rx_t source, char *data, size_t capacity, size_t *count)
{
    const char *text = s_rx[source];
    *count = text == NULL ? 0U : strlen(text);
    assert(*count <= capacity);
    if (*count != 0U)
    {
        memcpy(data, text, *count);
    }
    s_rx[source] = NULL;
    return FW_OK;
}

static fw_error_t drops(void *context, bsp_dosing_inputs_snapshot_t *snapshot)
{
    (void)context;
    *snapshot = (bsp_dosing_inputs_snapshot_t){.drop_count = s_drops, .count_valid = true};
    return FW_OK;
}

static uint32_t clock_ms(void *context)
{
    (void)context;
    return s_now;
}

static fw_error_t pump(void *context, service_dosing_pump_op_t op, uint32_t value,
                       fw_error_record_t *error)
{
    (void)context;
    (void)error;
    if (op == SERVICE_DOSING_PUMP_DIRECTION)
    {
        s_direction = value;
    }
    s_start += op == SERVICE_DOSING_PUMP_START;
    s_stop += op == SERVICE_DOSING_PUMP_STOP;
    return FW_OK;
}

static void tick(uint32_t ms)
{
    s_now += ms;
    app_dosing_control_step();
}

static void setup(void)
{
    const service_dosing_ops_t ops = {
        .inputs.read_snapshot = drops, .pump = pump, .now_ms = clock_ms
    };
    assert(service_dosing_init(&s_dosing, &ops) == FW_OK);
    s_now = s_drops = s_start = s_stop = s_direction = 0U;
    s_mv = 1120U;
    s_adc_valid = true;
    s_boot = false;
    memset(s_rx, 0, sizeof(s_rx));
    app_dosing_control_init(&s_dosing);
    tick(0U);
    assert(s_led == (DOSING_CONTROL_LED_LEVEL << 16U | DOSING_CONTROL_LED_LEVEL << 8U));
}

static void calibrate(uint32_t first, uint32_t remaining)
{
    s_boot = false;
    tick(10U);
    tick(DOSING_CONTROL_BUTTON_MS);
    s_boot = true;
    tick(10U);
    s_mv = first;
    tick(DOSING_CONTROL_BUTTON_MS);
    assert(s_led == DOSING_CONTROL_LED_LEVEL << 8U);
    tick(125U);
    assert(s_led == 0U);
    s_mv = remaining;
    for (unsigned i = 0; i < BUBBLE_CALIBRATION_SAMPLES; ++i)
    {
        tick(DOSING_CONTROL_SAMPLE_MS);
    }
    s_boot = false;
    tick(DOSING_CONTROL_BUTTON_MS);
}

static void expect_running(core_panel_rx_t source)
{
    s_rx[source] = "2";
    tick(10U);
    assert(s_dosing.result.state == SERVICE_DOSING_IDLE ||
           s_dosing.result.state == SERVICE_DOSING_DONE);
    s_rx[source] = "\r\n";
    tick(10U);
    assert(s_led == DOSING_CONTROL_LED_LEVEL);
    for (unsigned i = 0; i < 5U; ++i)
    {
        tick(10U);
    }
    assert(s_dosing.result.state == SERVICE_DOSING_RUNNING);
}

int main(void)
{
    fw_error_record_t error;
    setup();
    s_rx[CORE_PANEL_RX_USB] = "10\n";
    tick(10U);
    assert(s_start == 0U && s_stop == 0U);
    calibrate(1120U, 1121U + BUBBLE_CALIBRATION_SPREAD_MV);
    assert(app_dosing_control_get_error(&error) == FW_OK);
    assert(error.code == FW_ERR_APP_CALIBRATION && error.cause_code == FW_ERR_BUBBLE_UNSTABLE);
    assert(s_led == DOSING_CONTROL_LED_LEVEL << 16U);
    tick(500U);
    assert(s_led == 0U);
    calibrate(1120U, 1120U + BUBBLE_CALIBRATION_SPREAD_MV); /* 等于上限仍通过。 */
    assert(app_dosing_control_get_error(&error) == FW_OK && error.code == FW_OK);
    assert(s_led == DOSING_CONTROL_LED_LEVEL << 8U);

    s_rx[CORE_PANEL_RX_USB] = "10001\n-5\n0\n";
    tick(10U);
    assert(s_start == 0U);
    /* USB的换行不能提交UART0的半行。两个通道分别清理自己的命令。 */
    s_rx[CORE_PANEL_RX_UART0] = "9";
    s_rx[CORE_PANEL_RX_USB] = "\n";
    tick(10U);
    assert(s_dosing.result.state == SERVICE_DOSING_IDLE);
    s_rx[CORE_PANEL_RX_UART0] = "x\n";
    tick(10U);

    expect_running(CORE_PANEL_RX_USB);
    assert(s_dosing.result.target_drops == 2U);
    assert(s_start == 1U && s_direction == (DOSING_CONTROL_REVERSE != 0));
    s_rx[CORE_PANEL_RX_USB] = "33\n";
    s_rx[CORE_PANEL_RX_UART0] = "44\n";
    s_drops = 2U;
    tick(20U);
    assert(s_dosing.result.state == SERVICE_DOSING_TAIL);
    tick(DOSING_CONTROL_TAIL_QUIET_MS);
    tick(10U);
    tick(10U);
    assert(s_start == 2U && s_direction == (DOSING_CONTROL_REVERSE == 0));
    tick(499U);
    assert(s_dosing.result.state == SERVICE_DOSING_RETRACT_RUNNING);
    tick(1U);
    assert(s_dosing.result.state == SERVICE_DOSING_DONE && s_stop == 3U);
    tick(20U);
    assert(s_start == 2U && s_led == DOSING_CONTROL_LED_LEVEL << 8U);

    expect_running(CORE_PANEL_RX_UART0);
    s_mv = 1150U;
    tick(DOSING_CONTROL_TIMEOUT_MS - 1U);
    assert(s_dosing.result.state == SERVICE_DOSING_RUNNING);
    tick(1U);
    assert(s_dosing.result.state == SERVICE_DOSING_IDLE);
    assert(app_dosing_control_get_error(&error) == FW_OK);
    assert(error.code == FW_ERR_APP_CALIBRATION && error.cause_code == FW_ERR_DOSING_RUN_TIMEOUT);
    assert(error.request_id == 2U && s_stop == 5U);

    calibrate(1120U, 1120U);
    expect_running(CORE_PANEL_RX_USB);
    s_mv = 1921U; /* 基准1120上方801mV，有液但没有滴数。 */
    tick(DOSING_CONTROL_TIMEOUT_MS);
    assert(s_dosing.result.state == SERVICE_DOSING_FAULT);
    assert(app_dosing_control_get_error(&error) == FW_OK);
    assert(error.code == FW_ERR_APP_DOSE_EXIT);
    tick(10U);
    assert(s_led == DOSING_CONTROL_LED_LEVEL << 16U);
    puts("PASS control: calibration, LEDs, serial gating, recovery, count/ADC timeout split");
    return 0;
}
