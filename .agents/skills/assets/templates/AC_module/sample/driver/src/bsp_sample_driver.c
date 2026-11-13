/** @file bsp_sample_driver.c @brief 实现无HAL/OS依赖的实例化分步采样。 */
#include "bsp_sample_driver.h"

#include <limits.h>
#include <string.h>

#define SAMPLE_REG_ID UINT8_C(0x00)
#define SAMPLE_REG_TRIGGER UINT8_C(0x01)
#define SAMPLE_REG_DATA UINT8_C(0x02)

/** @brief 记录首个失败并进入FAULT，错误点由调用者宏捕获。 */
static fw_error_t sample_fail_at(bsp_sample_t *self, fw_error_t code,
                                 fw_error_t cause, const fw_error_record_t *origin,
                                 bsp_sample_stage_t stage, uint32_t now_ms,
                                 const char *file, const char *function, uint32_t line)
{
    if (self->state == BSP_SAMPLE_FAULT && self->error.code != FW_OK)
    {
        return self->error.code;
    }
    fw_error_capture_at(&self->error, code, cause, (uint32_t)self->state,
                        (uint16_t)stage, self->request_id, now_ms, file, function, line);
    if (origin != NULL && origin->code == cause && cause != FW_OK)
    {
        fw_error_set_origin(&self->error, origin);
    }
    /* 本驱动不持锁/CS/借用缓冲；有此类资源的实现必须在这里有界清理。
     * 清理失败写cleanup_code，保留上面的首个终态原因。 */
    self->state = BSP_SAMPLE_FAULT;
    return self->error.code;
}

/** @brief 在每个实际失败分支捕获文件、函数和行号，仅计算各参数一次。 */
#define SAMPLE_FAIL(self, code, cause, origin, stage, now_ms) \
    sample_fail_at((self), (code), (cause), (origin), (stage), (now_ms), \
                   __FILE__, __func__, (uint32_t)__LINE__)

/** @brief 计算示例帧CRC8；输入由内部固定长度缓冲保证有效。 */
static uint8_t sample_crc8(const uint8_t *data, size_t size)
{
    uint8_t crc = UINT8_C(0xFF);
    for (size_t i = 0U; i < size; ++i)
    {
        crc = (uint8_t)(crc ^ data[i]);
        for (unsigned bit = 0U; bit < 8U; ++bit)
        {
            crc = (crc & UINT8_C(0x80)) != 0U ?
                  (uint8_t)(((uint32_t)crc << 1U) ^ UINT32_C(0x31)) :
                  (uint8_t)((uint32_t)crc << 1U);
        }
    }
    return crc;
}

fw_error_t bsp_sample_init(bsp_sample_t *self, const bsp_sample_ops_t *ops,
                           const bsp_sample_config_t *config, uint32_t now_ms)
{
    if (self == NULL)
    {
        return FW_ERR_SENSOR_INVALID_ARG;
    }
    if (self->state == BSP_SAMPLE_WAITING)
    {
        return FW_ERR_SENSOR_BUSY;
    }
    /* 先复制输入，避免重新初始化时输入恰好来自self而被memset清除。 */
    bsp_sample_ops_t new_ops = ops != NULL ? *ops : (bsp_sample_ops_t){0};
    bsp_sample_config_t new_config = config != NULL ? *config : (bsp_sample_config_t){0};
    memset(self, 0, sizeof(*self));
    if (ops == NULL || config == NULL || new_ops.read_reg == NULL ||
        new_ops.write_reg == NULL || new_config.timeout_ms == 0U ||
        new_config.timeout_ms > (uint32_t)INT32_MAX ||
        new_config.conversion_ms >= new_config.timeout_ms ||
        new_config.io_timeout_ms == 0U ||
        new_config.io_timeout_ms > new_config.timeout_ms - new_config.conversion_ms)
    {
        return SAMPLE_FAIL(self, FW_ERR_SENSOR_INVALID_ARG, FW_OK, NULL,
                           BSP_SAMPLE_STAGE_INIT, now_ms);
    }
    self->ops = new_ops;
    self->config = new_config;
    uint8_t id = 0U;
    fw_error_record_t cause = {0};
    fw_error_t err = self->ops.read_reg(self->ops.context, SAMPLE_REG_ID, &id, 1U,
                                       self->config.io_timeout_ms, &cause);
    if (err != FW_OK)
    {
        return SAMPLE_FAIL(self, FW_ERR_SENSOR_ID_READ_FAILED, err, &cause,
                           BSP_SAMPLE_STAGE_INIT, now_ms);
    }
    if (id != self->config.expected_id)
    {
        return SAMPLE_FAIL(self, FW_ERR_SENSOR_ID_MISMATCH, FW_OK, NULL,
                           BSP_SAMPLE_STAGE_INIT, now_ms);
    }
    self->initialized = true;
    self->state = BSP_SAMPLE_IDLE;
    return FW_OK;
}

fw_error_t bsp_sample_start(bsp_sample_t *self, uint32_t request_id, uint32_t now_ms)
{
    if (self == NULL || request_id == 0U)
    {
        return FW_ERR_SENSOR_INVALID_ARG;
    }
    if (self->state == BSP_SAMPLE_FAULT)
    {
        return self->error.code;
    }
    if (!self->initialized)
    {
        return FW_ERR_SENSOR_NOT_READY;
    }
    if (self->state == BSP_SAMPLE_WAITING)
    {
        return request_id == self->request_id ? FW_OK : FW_ERR_SENSOR_BUSY;
    }
    if (self->state != BSP_SAMPLE_IDLE && self->state != BSP_SAMPLE_DONE &&
        self->state != BSP_SAMPLE_CANCELLED)
    {
        return SAMPLE_FAIL(self, FW_ERR_SENSOR_INVALID_STATE, FW_OK, NULL,
                           BSP_SAMPLE_STAGE_TRIGGER, now_ms);
    }
    if (request_id == self->request_id)
    {
        return FW_OK;
    }
    const uint8_t trigger = UINT8_C(1);
    fw_error_record_t cause = {0};
    fw_error_t err = self->ops.write_reg(self->ops.context, SAMPLE_REG_TRIGGER, &trigger,
                                        1U, self->config.io_timeout_ms, &cause);
    if (err == FW_ERR_PORT_BUSY)
    {
        return FW_ERR_SENSOR_BUSY;
    }
    self->request_id = request_id;
    self->started_ms = now_ms;
    memset(&self->error, 0, sizeof(self->error));
    if (err != FW_OK)
    {
        return SAMPLE_FAIL(self, FW_ERR_SENSOR_TRIGGER_UNKNOWN, err, &cause,
                           BSP_SAMPLE_STAGE_TRIGGER, now_ms);
    }
    self->state = BSP_SAMPLE_WAITING;
    return FW_OK;
}

fw_error_t bsp_sample_step(bsp_sample_t *self, uint32_t now_ms)
{
    if (self == NULL)
    {
        return FW_ERR_SENSOR_INVALID_ARG;
    }
    if (self->state == BSP_SAMPLE_FAULT)
    {
        return self->error.code;
    }
    if (!self->initialized)
    {
        return FW_ERR_SENSOR_NOT_READY;
    }
    switch (self->state)
    {
        case BSP_SAMPLE_IDLE:
        case BSP_SAMPLE_DONE:
        case BSP_SAMPLE_CANCELLED:
            return FW_OK;
        case BSP_SAMPLE_WAITING:
            break;
        default:
            return SAMPLE_FAIL(self, FW_ERR_SENSOR_INVALID_STATE, FW_OK, NULL,
                               BSP_SAMPLE_STAGE_VALIDATE, now_ms);
    }
    uint32_t elapsed_ms = now_ms - self->started_ms;
    if (elapsed_ms >= self->config.timeout_ms)
    {
        return SAMPLE_FAIL(self, FW_ERR_SENSOR_SAMPLE_TIMEOUT, FW_OK, NULL,
                           BSP_SAMPLE_STAGE_WAIT, now_ms);
    }
    if (elapsed_ms < self->config.conversion_ms)
    {
        return FW_OK;
    }
    uint32_t remaining_ms = self->config.timeout_ms - elapsed_ms;
    uint32_t io_ms = self->config.io_timeout_ms < remaining_ms ?
                     self->config.io_timeout_ms : remaining_ms;
    uint8_t bytes[3] = {0};
    fw_error_record_t cause = {0};
    fw_error_t err = self->ops.read_reg(self->ops.context, SAMPLE_REG_DATA, bytes,
                                       sizeof(bytes), io_ms, &cause);
    if (err == FW_ERR_PORT_BUSY)
    {
        return FW_OK;
    }
    if (err != FW_OK)
    {
        return SAMPLE_FAIL(self, FW_ERR_SENSOR_READ_FAILED, err, &cause,
                           BSP_SAMPLE_STAGE_READ, now_ms);
    }
    if (sample_crc8(bytes, 2U) != bytes[2])
    {
        return SAMPLE_FAIL(self, FW_ERR_SENSOR_CRC, FW_OK, NULL,
                           BSP_SAMPLE_STAGE_VALIDATE, now_ms);
    }
    uint16_t raw = (uint16_t)(((uint16_t)bytes[0] << 8U) | (uint16_t)bytes[1]);
    int32_t signed_raw = raw <= (uint16_t)INT16_MAX ? (int32_t)raw :
                         (int32_t)raw - INT32_C(65536);
    self->temperature_mc = signed_raw * INT32_C(10);
    self->state = BSP_SAMPLE_DONE;
    return FW_OK;
}

fw_error_t bsp_sample_cancel(bsp_sample_t *self, uint32_t now_ms)
{
    if (self == NULL)
    {
        return FW_ERR_SENSOR_INVALID_ARG;
    }
    if (self->state == BSP_SAMPLE_FAULT)
    {
        return self->error.code;
    }
    if (!self->initialized)
    {
        return FW_ERR_SENSOR_NOT_READY;
    }
    if (self->state != BSP_SAMPLE_IDLE && self->state != BSP_SAMPLE_WAITING &&
        self->state != BSP_SAMPLE_DONE && self->state != BSP_SAMPLE_CANCELLED)
    {
        return SAMPLE_FAIL(self, FW_ERR_SENSOR_INVALID_STATE, FW_OK, NULL,
                           BSP_SAMPLE_STAGE_VALIDATE, now_ms);
    }
    self->state = BSP_SAMPLE_CANCELLED;
    return FW_OK;
}

fw_error_t bsp_sample_get_result(const bsp_sample_t *self, int32_t *temperature_mc)
{
    if (self == NULL || temperature_mc == NULL)
    {
        return FW_ERR_SENSOR_INVALID_ARG;
    }
    if (self->state == BSP_SAMPLE_FAULT)
    {
        return self->error.code;
    }
    if (self->state == BSP_SAMPLE_WAITING)
    {
        return FW_ERR_SENSOR_BUSY;
    }
    if (!self->initialized || self->state != BSP_SAMPLE_DONE)
    {
        return FW_ERR_SENSOR_NOT_READY;
    }
    *temperature_mc = self->temperature_mc;
    return FW_OK;
}

fw_error_t bsp_sample_get_status(const bsp_sample_t *self, bsp_sample_state_t *state,
                                 fw_error_record_t *error)
{
    if (self == NULL || state == NULL || error == NULL)
    {
        return FW_ERR_SENSOR_INVALID_ARG;
    }
    *state = self->state;
    *error = self->error;
    return FW_OK;
}
