/** @file test_sample_driver.c @brief 用独立平台替身验证真实驱动模板的状态与错误语义。 */
#include "bsp_sample_driver.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint8_t id;
    uint8_t frame[3];
    fw_error_t id_error;
    fw_error_t sample_error;
    fw_error_t write_error;
    unsigned id_reads;
    unsigned sample_reads;
    unsigned writes;
    unsigned busy_reads;
    uint32_t last_timeout_ms;
} fake_port_t;

/** @brief 填入平台失败源头，验证上层包装不会丢失位置和原生码。 */
static void fake_error(fw_error_record_t *cause, fw_error_t code)
{
    fw_error_capture_at(cause, code, FW_OK, 0U, 7U, 0U, 9U,
                        __FILE__, __func__, (uint32_t)__LINE__);
    cause->native_status_valid = true;
    cause->native_status = 33;
}

static fw_error_t fake_read(void *context, uint8_t reg, uint8_t *data,
                            size_t size, uint32_t timeout_ms, fw_error_record_t *cause)
{
    fake_port_t *port = context;
    assert(timeout_ms > 0U);
    port->last_timeout_ms = timeout_ms;
    if (reg == 0U)
    {
        assert(size == 1U);
        ++port->id_reads;
        if (port->id_error != FW_OK)
        {
            fake_error(cause, port->id_error);
            return port->id_error;
        }
        *data = port->id;
    }
    else
    {
        assert(reg == 2U && size == sizeof(port->frame));
        ++port->sample_reads;
        if (port->busy_reads > 0U)
        {
            --port->busy_reads;
            return FW_ERR_PORT_BUSY;
        }
        if (port->sample_error != FW_OK)
        {
            fake_error(cause, port->sample_error);
            return port->sample_error;
        }
        memcpy(data, port->frame, size);
    }
    return FW_OK;
}

static fw_error_t fake_write(void *context, uint8_t reg, const uint8_t *data,
                             size_t size, uint32_t timeout_ms, fw_error_record_t *cause)
{
    fake_port_t *port = context;
    assert(reg == 1U && size == 1U && data[0] == 1U && timeout_ms > 0U);
    ++port->writes;
    if (port->write_error != FW_OK)
    {
        fake_error(cause, port->write_error);
    }
    return port->write_error;
}

static fake_port_t good_port(void)
{
    /* CRC是固定协议向量，不从被测驱动调用或复制其计算实现。 */
    return (fake_port_t){.id = 0x42U, .frame = {0x04U, 0xD2U, 0x64U}};
}

static bsp_sample_t initialized(fake_port_t *port)
{
    bsp_sample_t device = {0};
    const bsp_sample_ops_t ops = {.read_reg = fake_read, .write_reg = fake_write,
                                  .context = port};
    const bsp_sample_config_t config = {.expected_id = 0x42U, .conversion_ms = 10U,
                                        .timeout_ms = 40U, .io_timeout_ms = 4U};
    assert(bsp_sample_init(&device, &ops, &config, 100U) == FW_OK);
    return device;
}

static fw_error_record_t last_error(const bsp_sample_t *device, fw_error_t expected,
                                     bsp_sample_stage_t stage, const char *function)
{
    bsp_sample_state_t state;
    fw_error_record_t error;
    assert(bsp_sample_get_status(device, &state, &error) == FW_OK);
    assert(state == BSP_SAMPLE_FAULT && error.code == expected);
    assert(error.stage == (uint16_t)stage && error.line > 0U);
    assert(strstr(error.file, "bsp_sample_driver.c") != NULL);
    assert(strcmp(error.function, function) == 0);
    assert(strcmp(fw_error_code_name(expected), "FW_ERR_UNKNOWN") != 0);
    assert(strcmp(fw_error_code_text_zh(expected), "未注册错误码") != 0);
    return error;
}

static void success_and_instances(void)
{
    fake_port_t a = good_port();
    fake_port_t b = good_port();
    b.frame[0] = 0xFFU;
    b.frame[1] = 0xFFU;
    b.frame[2] = 0xACU;
    bsp_sample_t first = initialized(&a);
    bsp_sample_t second = initialized(&b);
    int32_t value = -999;
    assert(bsp_sample_start(&first, 1U, 100U) == FW_OK);
    assert(bsp_sample_start(&first, 1U, 101U) == FW_OK && a.writes == 1U);
    assert(bsp_sample_start(&first, 2U, 101U) == FW_ERR_SENSOR_BUSY);
    assert(bsp_sample_get_result(&first, &value) == FW_ERR_SENSOR_BUSY && value == -999);
    assert(bsp_sample_step(&first, 109U) == FW_OK && a.sample_reads == 0U);
    assert(bsp_sample_step(&first, 110U) == FW_OK);
    assert(bsp_sample_get_result(&first, &value) == FW_OK && value == 12340);
    assert(bsp_sample_start(&first, 1U, 111U) == FW_OK && a.writes == 1U);
    unsigned reads = a.sample_reads;
    assert(bsp_sample_step(&first, 200U) == FW_OK && a.sample_reads == reads);
    assert(bsp_sample_get_result(&second, &value) == FW_ERR_SENSOR_NOT_READY);
    assert(bsp_sample_start(&second, 5U, 200U) == FW_OK);
    assert(bsp_sample_step(&second, 210U) == FW_OK);
    assert(bsp_sample_get_result(&second, &value) == FW_OK && value == -10);
    assert(first.request_id == 1U && second.request_id == 5U);
    puts("PASS request acceptance/completion, immutable terminal, independent instances, signed data");
}

static void initialization_failures(void)
{
    fake_port_t port = good_port();
    bsp_sample_t device = {0};
    const bsp_sample_ops_t ops = {.read_reg = fake_read, .write_reg = fake_write,
                                  .context = &port};
    const bsp_sample_config_t config = {.expected_id = 0x42U, .conversion_ms = 10U,
                                        .timeout_ms = 40U, .io_timeout_ms = 4U};
    assert(bsp_sample_init(&device, NULL, &config, 1U) == FW_ERR_SENSOR_INVALID_ARG);
    (void)last_error(&device, FW_ERR_SENSOR_INVALID_ARG, BSP_SAMPLE_STAGE_INIT, "bsp_sample_init");
    port.id_error = FW_ERR_PORT_TIMEOUT;
    assert(bsp_sample_init(&device, &ops, &config, 2U) == FW_ERR_SENSOR_ID_READ_FAILED);
    fw_error_record_t error = last_error(&device, FW_ERR_SENSOR_ID_READ_FAILED,
                                         BSP_SAMPLE_STAGE_INIT, "bsp_sample_init");
    assert(error.cause_code == FW_ERR_PORT_TIMEOUT && error.native_status == 33);
    assert(error.origin_line > 0U && strstr(error.origin_file, "test_sample_driver.c") != NULL);
    port.id_error = FW_OK;
    port.id = 1U;
    assert(bsp_sample_init(&device, &ops, &config, 3U) == FW_ERR_SENSOR_ID_MISMATCH);
    (void)last_error(&device, FW_ERR_SENSOR_ID_MISMATCH, BSP_SAMPLE_STAGE_INIT, "bsp_sample_init");
    port.id = 0x42U;
    assert(bsp_sample_init(&device, &ops, &config, 4U) == FW_OK);
    puts("PASS initialization errors, native/source preservation and explicit recovery");
}

static void runtime_errors(void)
{
    fake_port_t port = good_port();
    bsp_sample_t device = initialized(&port);
    port.frame[2] ^= 1U;
    assert(bsp_sample_start(&device, 8U, 100U) == FW_OK);
    assert(bsp_sample_step(&device, 110U) == FW_ERR_SENSOR_CRC);
    fw_error_record_t crc = last_error(&device, FW_ERR_SENSOR_CRC,
                                       BSP_SAMPLE_STAGE_VALIDATE, "bsp_sample_step");
    assert(crc.request_id == 8U && crc.state == BSP_SAMPLE_WAITING && crc.timestamp_ms == 110U);
    int32_t output = 777;
    assert(bsp_sample_get_result(&device, &output) == FW_ERR_SENSOR_CRC && output == 777);
    unsigned writes = port.writes;
    unsigned reads = port.sample_reads;
    assert(bsp_sample_step(&device, 500U) == FW_ERR_SENSOR_CRC);
    assert(bsp_sample_start(&device, 9U, 500U) == FW_ERR_SENSOR_CRC);
    assert(port.writes == writes && port.sample_reads == reads && device.error.line == crc.line);
    port = good_port();
    device = initialized(&port);
    port.sample_error = FW_ERR_PORT_IO;
    assert(bsp_sample_start(&device, 10U, 100U) == FW_OK);
    assert(bsp_sample_step(&device, 110U) == FW_ERR_SENSOR_READ_FAILED);
    fw_error_record_t read = last_error(&device, FW_ERR_SENSOR_READ_FAILED,
                                        BSP_SAMPLE_STAGE_READ, "bsp_sample_step");
    assert(read.cause_code == FW_ERR_PORT_IO && read.native_status_valid);
    assert(read.native_status == 33 && strcmp(read.origin_function, "fake_error") == 0);
    puts("PASS CRC/read failure identity, stage, request, original cause and latched first error");
}

static void timeouts_and_busy(void)
{
    fake_port_t port = good_port();
    bsp_sample_t device = initialized(&port);
    port.busy_reads = 100U;
    assert(bsp_sample_start(&device, 1U, UINT32_MAX - 5U) == FW_OK);
    assert(bsp_sample_step(&device, 4U) == FW_OK);
    assert(bsp_sample_step(&device, 33U) == FW_OK && port.last_timeout_ms == 1U);
    assert(bsp_sample_step(&device, 34U) == FW_ERR_SENSOR_SAMPLE_TIMEOUT);
    fw_error_record_t error = last_error(&device, FW_ERR_SENSOR_SAMPLE_TIMEOUT,
                                         BSP_SAMPLE_STAGE_WAIT, "bsp_sample_step");
    assert(error.request_id == 1U && device.started_ms == UINT32_MAX - 5U);
    port = good_port();
    device = initialized(&port);
    port.write_error = FW_ERR_PORT_BUSY;
    assert(bsp_sample_start(&device, 3U, 100U) == FW_ERR_SENSOR_BUSY);
    assert(device.state == BSP_SAMPLE_IDLE && device.request_id == 0U);
    port.write_error = FW_ERR_PORT_TIMEOUT;
    assert(bsp_sample_start(&device, 3U, 100U) == FW_ERR_SENSOR_TRIGGER_UNKNOWN);
    fw_error_record_t unknown = last_error(&device, FW_ERR_SENSOR_TRIGGER_UNKNOWN,
                                           BSP_SAMPLE_STAGE_TRIGGER, "bsp_sample_start");
    assert(unknown.cause_code == FW_ERR_PORT_TIMEOUT);
    unsigned writes = port.writes;
    assert(bsp_sample_start(&device, 4U, 150U) == FW_ERR_SENSOR_TRIGGER_UNKNOWN);
    assert(port.writes == writes);
    puts("PASS total budget across uint32 wrap, bounded IO and no blind non-idempotent retries");
}

static void cancellation_and_illegal_state(void)
{
    fake_port_t port = good_port();
    bsp_sample_t device = initialized(&port);
    assert(bsp_sample_start(&device, 1U, 100U) == FW_OK);
    assert(bsp_sample_cancel(&device, 105U) == FW_OK);
    assert(bsp_sample_cancel(&device, 106U) == FW_OK);
    assert(bsp_sample_step(&device, 200U) == FW_OK && port.sample_reads == 0U);
    assert(bsp_sample_start(&device, 2U, 200U) == FW_OK);
    /* 显式注入非法状态，验证default终态具有具体失败位置。 */
    device.state = (bsp_sample_state_t)99;
    assert(bsp_sample_step(&device, 201U) == FW_ERR_SENSOR_INVALID_STATE);
    fw_error_record_t error = last_error(&device, FW_ERR_SENSOR_INVALID_STATE,
                                         BSP_SAMPLE_STAGE_VALIDATE, "bsp_sample_step");
    assert(error.state == 99U && error.request_id == 2U);
    puts("PASS idempotent cancel, restart boundary and readable illegal-state default");
}

static void error_registry_and_cleanup(void)
{
    const fw_error_t codes[] =
    {
        FW_OK, FW_ERR_PORT_TIMEOUT, FW_ERR_PORT_BUSY, FW_ERR_PORT_IO,
        FW_ERR_SENSOR_INVALID_ARG, FW_ERR_SENSOR_NOT_READY, FW_ERR_SENSOR_BUSY,
        FW_ERR_SENSOR_ID_READ_FAILED, FW_ERR_SENSOR_ID_MISMATCH,
        FW_ERR_SENSOR_TRIGGER_UNKNOWN, FW_ERR_SENSOR_READ_FAILED,
        FW_ERR_SENSOR_SAMPLE_TIMEOUT, FW_ERR_SENSOR_CRC, FW_ERR_SENSOR_INVALID_STATE
    };
    for (size_t i = 0U; i < sizeof(codes) / sizeof(codes[0]); ++i)
    {
        assert(strcmp(fw_error_code_name(codes[i]), "FW_ERR_UNKNOWN") != 0);
        assert(strcmp(fw_error_code_text_zh(codes[i]), "未注册错误码") != 0);
        for (size_t j = i + 1U; j < sizeof(codes) / sizeof(codes[0]); ++j)
        {
            assert(codes[i] != codes[j]);
        }
    }
    assert(strcmp(fw_error_code_name(UINT32_MAX), "FW_ERR_UNKNOWN") == 0);
    fw_error_record_t cause;
    fw_error_record_t outer;
    fw_error_capture_at(&cause, FW_ERR_PORT_TIMEOUT, FW_OK, 1U, 2U, 3U, 4U,
                        "port.c", "transfer", 12U);
    fw_error_capture_at(&outer, FW_ERR_SENSOR_READ_FAILED, cause.code, 2U, 3U, 4U, 5U,
                        "driver.c", "read", 30U);
    outer.cleanup_code = FW_ERR_PORT_IO;
    fw_error_set_origin(&outer, &cause);
    assert(outer.code == FW_ERR_SENSOR_READ_FAILED && outer.cleanup_code == FW_ERR_PORT_IO);
    assert(strcmp(outer.file, "driver.c") == 0 && strcmp(outer.origin_file, "port.c") == 0);
    assert(outer.line == 30U && outer.origin_line == 12U);
    fw_error_record_t next;
    fw_error_capture_at(&next, FW_ERR_SENSOR_TRIGGER_UNKNOWN, outer.code, 3U, 4U, 5U, 6U,
                        "service.c", "start", 44U);
    fw_error_set_origin(&next, &outer);
    assert(strcmp(next.file, "service.c") == 0 && strcmp(next.origin_file, "port.c") == 0);
    assert(next.cause_code == outer.code && next.origin_line == 12U);
    puts("PASS unique registry, Chinese explanations, cause propagation and separate cleanup error");
}

int main(void)
{
    success_and_instances();
    initialization_failures();
    runtime_errors();
    timeouts_and_busy();
    cancellation_and_illegal_state();
    error_registry_and_cleanup();
    return 0;
}
