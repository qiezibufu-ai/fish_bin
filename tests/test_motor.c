/** @file test_motor.c @brief 真实电机驱动与应用FSM的协议向量、故障注入及时间回绕测试。 */
#include "app_motor_test.h"
#include "bsp_kamoer_driver.h"
#include "osal_task.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/** @brief 单线程确定性总线；不用于证明RTOS并发安全。 */
typedef struct
{
    uint32_t now;
    unsigned calls;
    unsigned starts;
    unsigned stops;
    uint8_t last_tx[13];
    size_t last_bytes;
    fw_error_t io_error;
    bool corrupt_crc;
    bool wrong_echo;
    bool wrong_address;
    bool truncated;
    bool wrong_count;
    uint8_t exception;
} motor_bus_t;

static uint32_t mock_now(void *context)
{
    return ((motor_bus_t *)context)->now;
}

static void add_crc(uint8_t *frame, size_t bytes)
{
    uint16_t crc = bsp_kamoer_crc16(frame, bytes - 2U);
    frame[bytes - 2U] = (uint8_t)crc;
    frame[bytes - 1U] = (uint8_t)(crc >> 8U);
}

static fw_error_t mock_exchange(void *context, const uint8_t *tx, size_t tx_bytes,
                                uint8_t *rx, size_t capacity, size_t *rx_bytes,
                                uint32_t timeout_ms, fw_error_record_t *origin)
{
    motor_bus_t *bus = context;
    assert(capacity == 21U && timeout_ms > 0U);
    assert(bsp_kamoer_crc16(tx, tx_bytes) == 0U);
    ++bus->calls;
    memcpy(bus->last_tx, tx, tx_bytes);
    bus->last_bytes = tx_bytes;
    if (tx[1] == 5U && tx[3] == 1U)
    {
        if (tx[4] == 0xFFU)
        {
            ++bus->starts;
        }
        else
        {
            ++bus->stops;
        }
    }
    memset(origin, 0, sizeof(*origin));
    if (bus->io_error != FW_OK)
    {
        fw_error_capture_at(origin, bus->io_error, FW_OK, 1U, 1U, 0U, bus->now,
                            "mock_uart.c", "mock_exchange", 42U);
        origin->native_status = -123;
        origin->native_status_valid = true;
        *rx_bytes = 0U;
        return bus->io_error;
    }
    if (bus->exception != 0U)
    {
        rx[0] = tx[0];
        rx[1] = (uint8_t)(tx[1] | 0x80U);
        rx[2] = bus->exception;
        *rx_bytes = 5U;
    }
    else if (tx[1] == 0x04U)
    {
        /* 10rpm、0x12345678pulse、固件0102、硬件0304、序列89ABCDEF。 */
        const uint8_t feedback[21] = {
            1U, 4U, 16U, 0U, 0U, 3U, 0xE8U, 0x12U, 0x34U, 0x56U, 0x78U,
            1U, 2U, 3U, 4U, 0x89U, 0xABU, 0xCDU, 0xEFU, 0U, 0U
        };
        memcpy(rx, feedback, sizeof(feedback));
        rx[0] = tx[0];
        *rx_bytes = sizeof(feedback);
        if (bus->wrong_count)
        {
            rx[2] = 14U;
        }
    }
    else
    {
        memcpy(rx, tx, 6U);
        *rx_bytes = 8U;
        if (bus->wrong_echo)
        {
            rx[5] ^= 1U;
        }
    }
    if (bus->wrong_address)
    {
        rx[0] = 2U;
    }
    add_crc(rx, *rx_bytes);
    if (bus->corrupt_crc)
    {
        rx[*rx_bytes - 1U] ^= 1U;
    }
    if (bus->truncated)
    {
        *rx_bytes = 3U;
    }
    return FW_OK;
}

static void setup_motor(bsp_kamoer_t *motor, motor_bus_t *bus)
{
    memset(bus, 0, sizeof(*bus));
    bus->now = 100U;
    bsp_kamoer_ops_t ops = {mock_exchange, mock_now, bus};
    assert(bsp_kamoer_init(motor, &ops, 1U, 250U) == FW_OK);
}

static void test_motor_vectors(void)
{
    bsp_kamoer_t motor = {0};
    motor_bus_t bus;
    setup_motor(&motor, &bus);
    /* 独立黄金向量来自4460.5手册PDF第7/9页。 */
    const uint8_t start[8] = {1, 5, 0, 1, 0xFF, 0, 0xDD, 0xFA};
    const uint8_t stop[8] = {1, 5, 0, 1, 0, 0, 0x9C, 0x0A};
    const uint8_t speed[13] = {1, 0x10, 0x40, 9, 0, 2, 4, 0, 0, 0x75, 0x30, 0x24, 0x82};
    assert(bsp_kamoer_start(&motor) == FW_OK);
    assert(memcmp(bus.last_tx, start, sizeof(start)) == 0);
    assert(bsp_kamoer_stop(&motor) == FW_OK);
    assert(memcmp(bus.last_tx, stop, sizeof(stop)) == 0);
    assert(bsp_kamoer_set_speed(&motor, 30000U) == FW_OK);
    assert(memcmp(bus.last_tx, speed, sizeof(speed)) == 0);
    assert(bsp_kamoer_set_direction(&motor, BSP_KAMOER_CCW) == FW_OK);
    assert(bus.last_tx[3] == 2U && bus.last_tx[4] == 0xFFU);
    assert(bsp_kamoer_set_speed_mode(&motor) == FW_OK && bus.last_tx[3] == 3U);
    assert(bsp_kamoer_set_start_speed(&motor, 1000U) == FW_OK);
    assert(bus.last_tx[3] == 7U);
    unsigned calls = bus.calls;
    assert(bsp_kamoer_set_speed(&motor, 99U) == FW_ERR_KAMOER_INVALID_ARG);
    assert(bsp_kamoer_set_speed(&motor, 50001U) == FW_ERR_KAMOER_INVALID_ARG);
    assert(bsp_kamoer_set_start_speed(&motor, 50001U) == FW_ERR_KAMOER_INVALID_ARG);
    assert(bsp_kamoer_set_direction(&motor, (bsp_kamoer_direction_t)9) ==
           FW_ERR_KAMOER_INVALID_ARG);
    assert(bus.calls == calls && motor.state == BSP_KAMOER_READY);
    bsp_kamoer_feedback_t feedback = {0};
    assert(bsp_kamoer_read_feedback(&motor, &feedback) == FW_OK);
    assert(feedback.speed_centi_rpm == 1000U && feedback.position_pulse == 0x12345678U);
    assert(feedback.firmware == 0x0102U && feedback.hardware == 0x0304U);
    assert(feedback.serial == 0x89ABCDEFU && feedback.timestamp_ms == 100U);
}

static void test_motor_failures(void)
{
    bsp_kamoer_t motor = {0};
    motor_bus_t bus;
    setup_motor(&motor, &bus);
    bus.io_error = FW_ERR_PORT_TIMEOUT;
    assert(bsp_kamoer_start(&motor) == FW_ERR_KAMOER_EXECUTION_UNKNOWN);
    assert(motor.error.cause_code == FW_ERR_PORT_TIMEOUT);
    assert(motor.error.state == BSP_KAMOER_READY && motor.error.request_id == 1U);
    assert(motor.error.line > 0U && strstr(motor.error.file, "bsp_kamoer_driver.c") != NULL);
    assert(motor.error.native_status_valid && motor.error.native_status == -123);
    assert(motor.error.origin_line == 42U);
    unsigned calls = bus.calls;
    bsp_kamoer_feedback_t feedback = {.serial = 123U};
    assert(bsp_kamoer_start(&motor) == FW_ERR_KAMOER_EXECUTION_UNKNOWN);
    assert(bsp_kamoer_read_feedback(&motor, &feedback) == FW_ERR_KAMOER_EXECUTION_UNKNOWN);
    assert(bus.calls == calls && feedback.serial == 123U);
    assert(bsp_kamoer_stop(&motor) == FW_ERR_KAMOER_EXECUTION_UNKNOWN);
    assert(motor.error.cleanup_code == FW_ERR_KAMOER_EXECUTION_UNKNOWN);
    assert(motor.error.request_id == 1U && motor.error.origin_line == 42U);
    bus.io_error = FW_OK;
    assert(bsp_kamoer_stop(&motor) == FW_OK && motor.state == BSP_KAMOER_FAULT);
    for (unsigned kind = 0U; kind < 6U; ++kind)
    {
        setup_motor(&motor, &bus);
        if (kind == 0U) { bus.corrupt_crc = true; }
        if (kind == 1U) { bus.wrong_address = true; }
        if (kind == 2U) { bus.truncated = true; }
        if (kind == 3U) { bus.wrong_count = true; }
        if (kind == 4U) { bus.exception = 2U; }
        if (kind == 5U) { bus.io_error = FW_ERR_PORT_TIMEOUT; }
        fw_error_t expected = kind == 0U ? FW_ERR_KAMOER_CRC :
                              kind == 4U ? FW_ERR_KAMOER_EXCEPTION :
                              kind == 5U ? FW_ERR_KAMOER_TIMEOUT : FW_ERR_KAMOER_RESPONSE;
        assert(bsp_kamoer_read_feedback(&motor, &feedback) == expected);
        assert(feedback.serial == 123U);
        if (kind == 4U)
        {
            assert(motor.error.native_status == 2 && motor.error.native_status_valid);
        }
    }
    setup_motor(&motor, &bus);
    bus.exception = 2U;
    assert(bsp_kamoer_start(&motor) == FW_ERR_KAMOER_EXCEPTION);
    assert(motor.error.native_status_valid && motor.error.native_status == 2);
    setup_motor(&motor, &bus);
    bus.wrong_echo = true;
    assert(bsp_kamoer_start(&motor) == FW_ERR_KAMOER_EXECUTION_UNKNOWN);
    assert(motor.error.cause_code == FW_ERR_KAMOER_RESPONSE);
    setup_motor(&motor, &bus);
    motor.state = (bsp_kamoer_state_t)77;
    assert(bsp_kamoer_start(&motor) == FW_ERR_KAMOER_INVALID_STATE);
    assert(motor.error.state == 77U && bus.calls == 0U);
    assert(strcmp(fw_error_code_name(motor.error.code), "FW_ERR_UNKNOWN") != 0);
}

static void setup_app(app_motor_test_t *app, motor_bus_t *bus)
{
    memset(app, 0, sizeof(*app));
    setup_motor(&app->motor, bus);
}

static void test_motor_application(void)
{
    app_motor_test_t app;
    motor_bus_t bus;
    setup_app(&app, &bus);
    assert(app_motor_test_step(&app) == FW_OK && bus.calls == 0U);
    assert(app_motor_test_start(&app, BSP_KAMOER_CW, 1000U, 10U) == FW_OK);
    assert(app_motor_test_start(&app, BSP_KAMOER_CW, 1000U, 10U) == FW_ERR_KAMOER_BUSY);
    while (app.state != APP_MOTOR_RUNNING)
    {
        assert(app_motor_test_step(&app) == FW_OK);
    }
    assert(bus.starts == 1U && bus.stops == 1U && app.feedback_valid);
    app.started_ms = osal_task_now_ms() - 11U; /* 同时覆盖uint32回绕。 */
    assert(app_motor_test_step(&app) == FW_OK);
    assert(app.state == APP_MOTOR_STOP_ACKED && app.stop_acked && bus.stops == 2U);
    unsigned calls = bus.calls;
    assert(app_motor_test_step(&app) == FW_OK && bus.calls == calls);
    assert(app_motor_test_start(&app, BSP_KAMOER_CCW, 2000U, 100U) == FW_OK);
    assert(app_motor_test_cancel(&app) == FW_OK && app.state == APP_MOTOR_CANCELLED);
    calls = bus.calls;
    assert(app_motor_test_step(&app) == FW_OK && bus.calls == calls);
    /* 启动超时：仅一次START、一次清理STOP，FAULT轮询永远不会重启动。 */
    setup_app(&app, &bus);
    assert(app_motor_test_start(&app, BSP_KAMOER_CW, 1000U, 10U) == FW_OK);
    while (app.state != APP_MOTOR_START)
    {
        assert(app_motor_test_step(&app) == FW_OK);
    }
    bus.io_error = FW_ERR_PORT_TIMEOUT;
    assert(app_motor_test_step(&app) == FW_ERR_APP_MOTOR_TEST_FAILED);
    assert(app.state == APP_MOTOR_FAULT && !app.stop_acked);
    assert(app.error.cause_code == FW_ERR_KAMOER_EXECUTION_UNKNOWN);
    assert(bus.starts == 1U && bus.stops == 2U);
    assert(app.error.state == APP_MOTOR_START && app.error.cleanup_code != FW_OK);
    assert(app.error.origin_line == 42U && app.error.native_status == -123);
    calls = bus.calls;
    app_motor_test_result_t result;
    assert(app_motor_test_get_result(&app, &result) == FW_OK);
    assert(result.state == APP_MOTOR_FAULT && result.error.code == FW_ERR_APP_MOTOR_TEST_FAILED);
    assert(app_motor_test_step(&app) == app.error.code && bus.calls == calls);
    assert(app_motor_test_start(&app, BSP_KAMOER_CW, 1000U, 10U) == app.error.code);
    bus.io_error = FW_OK;
    assert(app_motor_test_cancel(&app) == FW_OK && app.stop_acked);
    assert(app.state == APP_MOTOR_FAULT && app.error.code == FW_ERR_APP_MOTOR_TEST_FAILED);
    setup_app(&app, &bus);
    app.state = (app_motor_test_state_t)99;
    assert(app_motor_test_step(&app) == FW_ERR_APP_MOTOR_INVALID_STATE);
    assert(app.error.state == 99U && app.state == APP_MOTOR_FAULT);
}

void test_motor_apis(void)
{
    test_motor_vectors();
    test_motor_failures();
    test_motor_application();
    puts("PASS AC/AA motor: manual vectors, feedback, CRC/echo/exception, unknown writes, cleanup, cancellation, wrap");
}
