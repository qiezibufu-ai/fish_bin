/** @file app_motor_test.c @brief 有界电机台架测试，每个阶段只执行一次设备事务。 */
#include "app_motor_test.h"
#include "bsp_adapter_port_kamoer.h"
#include "motor_test_config.h"
#include "osal_task.h"
#include <string.h>

static fw_error_t motor_test_fail_at(app_motor_test_t *self, fw_error_t code,
                                     bool cleanup, const char *file,
                                     const char *function, uint32_t line)
{
    if (self->state == APP_MOTOR_FAULT)
    {
        return self->error.code;
    }
    fw_error_t terminal = code == FW_ERR_APP_MOTOR_INVALID_STATE ?
                          FW_ERR_APP_MOTOR_INVALID_STATE : FW_ERR_APP_MOTOR_TEST_FAILED;
    fw_error_capture_at(&self->error, terminal, code, (uint32_t)self->state,
                        (uint16_t)self->state, self->motor.request_id,
                        osal_task_now_ms(), file, function, line);
    if (self->motor.error.code != FW_OK)
    {
        fw_error_set_origin(&self->error, &self->motor.error);
    }
    self->state = APP_MOTOR_FAULT;
    if (cleanup)
    {
        self->error.cleanup_code = bsp_kamoer_stop(&self->motor);
        self->stop_acked = self->error.cleanup_code == FW_OK;
    }
    return self->error.code;
}
#define MOTOR_TEST_FAIL(s, code, cleanup) \
    motor_test_fail_at((s), (code), (cleanup), __FILE__, __func__, (uint32_t)__LINE__)

fw_error_t app_motor_test_init(app_motor_test_t *self)
{
    if (self == NULL)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    memset(self, 0, sizeof(*self));
    fw_error_t error = bsp_adapter_port_kamoer_bind(&self->motor);
    if (error != FW_OK)
    {
        return MOTOR_TEST_FAIL(self, error, false);
    }
    return FW_OK;
}

fw_error_t app_motor_test_start(app_motor_test_t *self, bsp_kamoer_direction_t direction,
                               uint32_t speed_centi_rpm, uint32_t duration_ms)
{
    if (self == NULL || (direction != BSP_KAMOER_CW && direction != BSP_KAMOER_CCW) ||
        speed_centi_rpm < 100U || speed_centi_rpm > 50000U ||
        duration_ms == 0U || duration_ms > 60000U)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    if (self->motor.state == BSP_KAMOER_UNINITIALIZED)
    {
        return FW_ERR_KAMOER_NOT_READY;
    }
    switch (self->state)
    {
        case APP_MOTOR_IDLE:
        case APP_MOTOR_STOP_ACKED:
        case APP_MOTOR_CANCELLED:
            break;
        case APP_MOTOR_FAULT:
            return self->error.code;
        case APP_MOTOR_PROBE:
        case APP_MOTOR_INITIAL_STOP:
        case APP_MOTOR_DIRECTION:
        case APP_MOTOR_MODE:
        case APP_MOTOR_START_SPEED:
        case APP_MOTOR_SPEED:
        case APP_MOTOR_START:
        case APP_MOTOR_RUNNING:
            return FW_ERR_KAMOER_BUSY;
        default:
            return MOTOR_TEST_FAIL(self, FW_ERR_APP_MOTOR_INVALID_STATE, true);
    }
    self->duration_ms = duration_ms;
    self->speed_centi_rpm = speed_centi_rpm;
    self->direction = direction;
    self->stop_acked = false;
    self->feedback_valid = false;
    memset(&self->error, 0, sizeof(self->error));
    self->state = APP_MOTOR_PROBE;
    return FW_OK;
}

fw_error_t app_motor_test_step(app_motor_test_t *self)
{
    if (self == NULL)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    fw_error_t error;
    app_motor_test_state_t next = self->state;
    switch (self->state)
    {
        case APP_MOTOR_IDLE:
        case APP_MOTOR_STOP_ACKED:
        case APP_MOTOR_CANCELLED:
            return FW_OK;
        case APP_MOTOR_FAULT:
            return self->error.code;
        case APP_MOTOR_PROBE:
            error = bsp_kamoer_read_feedback(&self->motor, &self->feedback);
            if (error == FW_OK)
            {
                self->feedback_valid = true;
            }
            next = APP_MOTOR_INITIAL_STOP;
            break;
        case APP_MOTOR_INITIAL_STOP:
            error = bsp_kamoer_stop(&self->motor);
            next = APP_MOTOR_DIRECTION;
            break;
        case APP_MOTOR_DIRECTION:
            error = bsp_kamoer_set_direction(&self->motor, self->direction);
            next = APP_MOTOR_MODE;
            break;
        case APP_MOTOR_MODE:
            error = bsp_kamoer_set_speed_mode(&self->motor);
            next = APP_MOTOR_START_SPEED;
            break;
        case APP_MOTOR_START_SPEED:
        {
            uint32_t start_speed = APP_MOTOR_TEST_START_SPEED_CENTI_RPM;
            if (start_speed > self->speed_centi_rpm)
            {
                start_speed = self->speed_centi_rpm;
            }
            error = bsp_kamoer_set_start_speed(&self->motor, start_speed);
            next = APP_MOTOR_SPEED;
            break;
        }
        case APP_MOTOR_SPEED:
            error = bsp_kamoer_set_speed(&self->motor, self->speed_centi_rpm);
            next = APP_MOTOR_START;
            break;
        case APP_MOTOR_START:
            error = bsp_kamoer_start(&self->motor);
            if (error == FW_OK)
            {
                self->started_ms = osal_task_now_ms();
            }
            next = APP_MOTOR_RUNNING;
            break;
        case APP_MOTOR_RUNNING:
            if ((uint32_t)(osal_task_now_ms() - self->started_ms) >= self->duration_ms)
            {
                error = bsp_kamoer_stop(&self->motor);
                if (error == FW_OK)
                {
                    self->stop_acked = true;
                    self->state = APP_MOTOR_STOP_ACKED;
                    return FW_OK;
                }
            }
            else
            {
                error = bsp_kamoer_read_feedback(&self->motor, &self->feedback);
                if (error == FW_OK)
                {
                    self->feedback_valid = true;
                    return FW_OK;
                }
            }
            break;
        default:
            return MOTOR_TEST_FAIL(self, FW_ERR_APP_MOTOR_INVALID_STATE, true);
    }
    if (error != FW_OK)
    {
        return MOTOR_TEST_FAIL(self, error, true);
    }
    self->state = next;
    return FW_OK;
}

fw_error_t app_motor_test_cancel(app_motor_test_t *self)
{
    if (self == NULL)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    fw_error_t error = bsp_kamoer_stop(&self->motor);
    self->stop_acked = error == FW_OK;
    if (self->state == APP_MOTOR_FAULT)
    {
        self->error.cleanup_code = error;
        return error;
    }
    if (error != FW_OK)
    {
        return MOTOR_TEST_FAIL(self, error, false);
    }
    self->state = APP_MOTOR_CANCELLED;
    return FW_OK;
}

fw_error_t app_motor_test_get_result(const app_motor_test_t *self,
                                    app_motor_test_result_t *result)
{
    if (self == NULL || result == NULL)
    {
        return FW_ERR_KAMOER_INVALID_ARG;
    }
    *result = (app_motor_test_result_t){
        .state = self->state,
        .error = self->error,
        .feedback = self->feedback,
        .feedback_valid = self->feedback_valid,
        .stop_acked = self->stop_acked
    };
    return FW_OK;
}
