/** @file bsp_kamoer_driver.c @brief 4460.5协议及ACK/未知结果诊断，不自动重发。 */
#include "bsp_kamoer_driver.h"
#include <string.h>

uint16_t bsp_kamoer_crc16(const uint8_t *data, size_t bytes)
{
    uint16_t crc = 0xFFFFU;
    if (data == NULL && bytes != 0U)
    {
        return 0U;
    }
    for (size_t i = 0U; i < bytes; ++i)
    {
        crc ^= data[i];
        for (unsigned bit = 0U; bit < 8U; ++bit)
        {
            crc = (crc & 1U) != 0U ? (uint16_t)((crc >> 1U) ^ 0xA001U) :
                                    (uint16_t)(crc >> 1U);
        }
    }
    return crc;
}

static void put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8U);
    p[1] = (uint8_t)value;
}

static uint32_t get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
           ((uint32_t)p[2] << 8U) | p[3];
}

static fw_error_t kamoer_fail_at(bsp_kamoer_t *self, fw_error_t code, fw_error_t cause,
                                 const fw_error_record_t *origin, uint8_t function_code,
                                 int32_t native, const char *file,
                                 const char *function, uint32_t line)
{
    if (self->state == BSP_KAMOER_FAULT)
    {
        self->error.cleanup_code = code;
        return code;
    }
    fw_error_capture_at(&self->error, code, cause, (uint32_t)self->state,
                        function_code, self->request_id,
                        self->ops.now_ms != NULL ? self->ops.now_ms(self->ops.context) : 0U,
                        file, function, line);
    if (origin != NULL && origin->code != FW_OK)
    {
        fw_error_set_origin(&self->error, origin);
    }
    if (native != 0)
    {
        self->error.native_status = native;
        self->error.native_status_valid = true;
    }
    self->state = BSP_KAMOER_FAULT;
    return code;
}
#define KAMOER_FAIL(s, c, cause, origin, fc, native) \
    kamoer_fail_at((s), (c), (cause), (origin), (fc), (native), \
                    __FILE__, __func__, (uint32_t)__LINE__)

fw_error_t bsp_kamoer_init(bsp_kamoer_t *self, const bsp_kamoer_ops_t *ops,
                          uint8_t address, uint32_t timeout_ms)
{
    if (self == NULL || ops == NULL || ops->exchange == NULL || ops->now_ms == NULL ||
        address == 0U || address > 247U || timeout_ms == 0U || timeout_ms > 2000U)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    bsp_kamoer_ops_t copy = *ops;
    memset(self, 0, sizeof(*self));
    self->ops = copy;
    self->address = address;
    self->timeout_ms = timeout_ms;
    self->state = BSP_KAMOER_READY;
    return FW_OK;
}

/** @brief 单次协议事务；任何写入的不完整/无效应答都保守报告结果未知。 */
static fw_error_t transact(bsp_kamoer_t *self, uint8_t *tx, size_t bytes,
                           uint8_t *rx, size_t expected, bool stop)
{
    if (self == NULL)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    switch (self->state)
    {
        case BSP_KAMOER_UNINITIALIZED:
            return FW_ERR_KAMOER_NOT_READY;
        case BSP_KAMOER_READY:
            break;
        case BSP_KAMOER_FAULT:
            if (!stop)
            {
                return self->error.code;
            }
            break;
        default:
            return KAMOER_FAIL(self, FW_ERR_KAMOER_INVALID_STATE, FW_OK, NULL, tx[1], 0);
    }
    tx[0] = self->address;
    uint16_t crc = bsp_kamoer_crc16(tx, bytes - 2U);
    tx[bytes - 2U] = (uint8_t)crc;
    tx[bytes - 1U] = (uint8_t)(crc >> 8U);
    ++self->request_id;
    size_t received = 0U;
    fw_error_record_t origin = {0};
    fw_error_t cause = self->ops.exchange(self->ops.context, tx, bytes, rx, 21U,
                                          &received, self->timeout_ms, &origin);
    bool write = tx[1] == 0x05U || tx[1] == 0x10U;
    fw_error_t code = FW_OK;
    int32_t exception = 0;
    if (cause != FW_OK)
    {
        code = (cause == FW_ERR_PORT_TIMEOUT || cause == FW_ERR_UART_TIMEOUT) ?
               FW_ERR_KAMOER_TIMEOUT : FW_ERR_KAMOER_IO;
    }
    else if (received < 5U || received > 21U)
    {
        code = FW_ERR_KAMOER_RESPONSE;
    }
    else if (bsp_kamoer_crc16(rx, received) != 0U)
    {
        code = FW_ERR_KAMOER_CRC;
    }
    else if (rx[0] != self->address)
    {
        code = FW_ERR_KAMOER_RESPONSE;
    }
    else if (rx[1] == (uint8_t)(tx[1] | 0x80U) && received == 5U)
    {
        code = FW_ERR_KAMOER_EXCEPTION;
        exception = rx[2];
    }
    else if (rx[1] != tx[1] || received != expected)
    {
        code = FW_ERR_KAMOER_RESPONSE;
    }
    else if (write && memcmp(tx, rx, 6U) != 0)
    {
        code = FW_ERR_KAMOER_RESPONSE;
    }
    else if (!write && rx[2] != expected - 5U)
    {
        code = FW_ERR_KAMOER_RESPONSE;
    }
    if (code != FW_OK)
    {
        if (cause == FW_OK)
        {
            cause = code;
        }
        if (write && code != FW_ERR_KAMOER_EXCEPTION)
        {
            code = FW_ERR_KAMOER_EXECUTION_UNKNOWN;
        }
        return KAMOER_FAIL(self, code, cause, &origin, tx[1], exception);
    }
    return FW_OK;
}

static fw_error_t write_coil(bsp_kamoer_t *self, uint16_t address, bool value, bool stop)
{
    uint8_t tx[8] = {0U, 0x05U};
    uint8_t rx[21] = {0};
    put_u16(&tx[2], address);
    put_u16(&tx[4], value ? 0xFF00U : 0U);
    return transact(self, tx, sizeof(tx), rx, 8U, stop);
}

static fw_error_t write_u32(bsp_kamoer_t *self, uint16_t address, uint32_t value)
{
    uint8_t tx[13] = {0U, 0x10U, 0U, 0U, 0U, 2U, 4U};
    uint8_t rx[21] = {0};
    put_u16(&tx[2], address);
    put_u16(&tx[7], (uint16_t)(value >> 16U));
    put_u16(&tx[9], (uint16_t)value);
    return transact(self, tx, sizeof(tx), rx, 8U, false);
}

fw_error_t bsp_kamoer_set_direction(bsp_kamoer_t *self, bsp_kamoer_direction_t direction)
{
    if (direction != BSP_KAMOER_CW && direction != BSP_KAMOER_CCW)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    return write_coil(self, 0x0002U, direction == BSP_KAMOER_CCW, false);
}

fw_error_t bsp_kamoer_set_speed_mode(bsp_kamoer_t *self)
{
    return write_coil(self, 0x0003U, false, false);
}

fw_error_t bsp_kamoer_set_start_speed(bsp_kamoer_t *self, uint32_t speed_centi_rpm)
{
    if (speed_centi_rpm > 50000U)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    return write_u32(self, 0x4007U, speed_centi_rpm);
}

fw_error_t bsp_kamoer_set_speed(bsp_kamoer_t *self, uint32_t speed_centi_rpm)
{
    if (speed_centi_rpm < 100U || speed_centi_rpm > 50000U)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    return write_u32(self, 0x4009U, speed_centi_rpm);
}

fw_error_t bsp_kamoer_start(bsp_kamoer_t *self)
{
    return write_coil(self, 0x0001U, true, false);
}

fw_error_t bsp_kamoer_stop(bsp_kamoer_t *self)
{
    return write_coil(self, 0x0001U, false, true);
}

fw_error_t bsp_kamoer_read_feedback(bsp_kamoer_t *self, bsp_kamoer_feedback_t *feedback)
{
    if (feedback == NULL)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    uint8_t tx[8] = {0U, 0x04U, 0x30U, 0x01U, 0U, 8U};
    uint8_t rx[21] = {0};
    fw_error_t error = transact(self, tx, sizeof(tx), rx, sizeof(rx), false);
    if (error == FW_OK)
    {
        *feedback = (bsp_kamoer_feedback_t){
            .speed_centi_rpm = get_u32(&rx[3]),
            .position_pulse = get_u32(&rx[7]),
            .firmware = (uint16_t)(((uint16_t)rx[11] << 8U) | rx[12]),
            .hardware = (uint16_t)(((uint16_t)rx[13] << 8U) | rx[14]),
            .serial = get_u32(&rx[15]),
            .timestamp_ms = self->ops.now_ms(self->ops.context)
        };
    }
    return error;
}

fw_error_t bsp_kamoer_get_status(const bsp_kamoer_t *self, bsp_kamoer_state_t *state,
                                fw_error_record_t *error)
{
    if (self == NULL || state == NULL || error == NULL)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    *state = self->state;
    *error = self->error;
    return FW_OK;
}
