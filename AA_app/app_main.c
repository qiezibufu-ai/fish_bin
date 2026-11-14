/**
 * @file app_main.c
 * @brief 实现应用层组合根：统一初始化平台端口、管理任务注册表并传播底层故障源头。
 */
#include "app_main.h"

#include "board_config.h"
#include "dosing_test_config.h"
#include "drop_count_test_config.h"
#if APP_DROP_COUNT_TEST_ENABLE || APP_DOSING_TEST_ENABLE
#include "bsp_adapter_port_drop_count.h"
#include "pcnt_port.h"
#include <inttypes.h>
#include <stdio.h>
#endif
#if APP_DOSING_TEST_ENABLE || !APP_DROP_COUNT_TEST_ENABLE
#include "motor_config.h"
#include "uart_port.h"
#endif
#if APP_DOSING_TEST_ENABLE
#include "bsp_adapter_port_kamoer.h"
#include "service_dosing.h"
#elif !APP_DROP_COUNT_TEST_ENABLE
#include "app_motor_test.h"
#include "motor_test_config.h"
#endif
#include "gpio_port.h"
#include "osal.h"

#include <stddef.h>
#include <string.h>

/** @brief 组合根管理的任务枚举。 */
typedef enum
{
    APP_TASK_BOOTSTRAP = 0, /**< 系统心跳与诊断监控任务。 */
    APP_TASK_COUNT          /**< 任务总数。 */
} app_task_id_t;

/** @brief 任务静态规格描述符，栈大小统一以 bytes 为单位。 */
typedef struct
{
    const char *name;           /**< 任务名称。 */
    osal_task_entry entry;      /**< 任务入口函数。 */
    size_t stack_bytes;         /**< 任务栈容量，单位bytes。 */
    osal_priority_t priority;   /**< 任务优先级。 */
    osal_task_handle_t *handle; /**< 任务句柄存储位置。 */
} app_task_spec_t;

#if APP_DROP_COUNT_TEST_ENABLE || APP_DOSING_TEST_ENABLE
static const bsp_dosing_inputs_ops_t s_drop_inputs = {
    .read_snapshot = bsp_adapter_port_drop_count_read,
    .context = NULL
};
#endif
#if APP_DOSING_TEST_ENABLE
static bsp_kamoer_t s_dosing_motor;
static service_dosing_t s_dosing;
#elif !APP_DROP_COUNT_TEST_ENABLE
static app_motor_test_t s_motor_test;
#endif

static app_state_t s_app_state = APP_STATE_UNINITIALIZED;
static fw_error_record_t s_app_error = {0};
static osal_task_handle_t s_bootstrap_task_handle = NULL;

#if APP_DOSING_TEST_ENABLE
static fw_error_t app_dosing_pump(void *context, service_dosing_pump_op_t op,
                                  uint32_t value, fw_error_record_t *error)
{
    bsp_kamoer_t *motor = context;
    fw_error_t err;
    switch (op)
    {
        case SERVICE_DOSING_PUMP_STOP:
            err = bsp_kamoer_stop(motor);
            break;
        case SERVICE_DOSING_PUMP_DIRECTION:
            err = bsp_kamoer_set_direction(motor, value == 0U ? BSP_KAMOER_CW : BSP_KAMOER_CCW);
            break;
        case SERVICE_DOSING_PUMP_SPEED_MODE:
            err = bsp_kamoer_set_speed_mode(motor);
            break;
        case SERVICE_DOSING_PUMP_START_SPEED:
            err = bsp_kamoer_set_start_speed(motor, value);
            break;
        case SERVICE_DOSING_PUMP_SPEED:
            err = bsp_kamoer_set_speed(motor, value);
            break;
        case SERVICE_DOSING_PUMP_START:
            err = bsp_kamoer_start(motor);
            break;
        default:
            return FW_ERR_DOSING_ARG;
    }
    if (err != FW_OK)
    {
        bsp_kamoer_state_t state;
        (void)bsp_kamoer_get_status(motor, &state, error);
    }
    return err;
}

static void app_dosing_input_error(void *context, fw_error_record_t *error)
{
    (void)context;
    (void)core_pcnt_get_error(error);
}

static uint32_t app_dosing_now(void *context)
{
    (void)context;
    return osal_task_now_ms();
}

static fw_error_t app_dosing_bind(void)
{
    fw_error_t err = bsp_adapter_port_kamoer_bind(&s_dosing_motor);
    if (err != FW_OK)
    {
        return err;
    }
    const service_dosing_ops_t ops = {
        .inputs = s_drop_inputs,
        .pump = app_dosing_pump,
        .pump_context = &s_dosing_motor,
        .now_ms = app_dosing_now,
        .input_error = app_dosing_input_error
    };
    return service_dosing_init(&s_dosing, &ops);
}

static bool app_dosing_submit(void)
{
    const service_dosing_request_t request = {
        .target_drops = APP_DOSING_TEST_TARGET_DROPS,
        .speed_centi_rpm = APP_DOSING_TEST_SPEED_CENTI_RPM,
        .reverse = APP_DOSING_TEST_REVERSE != 0,
        .no_drop_ms = APP_DOSING_TEST_NO_DROP_MS,
        .max_run_ms = APP_DOSING_TEST_MAX_RUN_MS,
        .tail_quiet_ms = APP_DOSING_TEST_TAIL_QUIET_MS,
        .tail_max_ms = APP_DOSING_TEST_TAIL_MAX_MS
    };
    fw_error_t err = service_dosing_start(&s_dosing, &request);
    if (err != FW_OK)
    {
        printf("dosing submit=%s\n", fw_error_code_name(err));
    }
    return err == FW_OK;
}

static bool app_dosing_advance(void)
{
    (void)service_dosing_step(&s_dosing);
    service_dosing_result_t result;
    if (service_dosing_get_result(&s_dosing, &result) != FW_OK ||
        (result.state != SERVICE_DOSING_DONE && result.state != SERVICE_DOSING_CANCELLED &&
         result.state != SERVICE_DOSING_FAULT))
    {
        return true;
    }
    /* 仅终态输出，串口日志不会插入运行到停泵的控制路径。 */
    const char *state = result.state == SERVICE_DOSING_DONE ? "DONE" :
                        result.state == SERVICE_DOSING_CANCELLED ? "CANCELLED" : "FAULT";
    printf("dosing: state=%s target=%" PRIu32 " stop=%" PRIu32 " total=%" PRIu32
           " count_valid=%u stop_acked=%u error=%s cause=%s cleanup=%s\n",
           state, result.target_drops, result.stop_drops, result.drops,
           (unsigned)result.count_valid, (unsigned)result.stop_acked,
           fw_error_code_name(result.error.code), fw_error_code_name(result.error.cause_code),
           fw_error_code_name(result.error.cleanup_code));
    return false;
}
#endif

/** @brief 按构建配置运行滴定、计数或电机台架；三种模式互斥。 */
static void bootstrap_task_entry(void *argument)
{
    (void)argument;
#if APP_DOSING_TEST_ENABLE
    bool active = app_dosing_submit();
    const uint32_t period_ms = APP_DOSING_TEST_PERIOD_MS;
#elif APP_DROP_COUNT_TEST_ENABLE
    uint32_t last_count = 0U;
    bool reported = false;
    bool sampling = true;
    const uint32_t period_ms = APP_DROP_COUNT_REPORT_PERIOD_MS;
    printf("drop_count: GPIO%d falling edge; motor commands off\n", BOARD_GPIO_DROP_COUNT_PIN);
#else
    const uint32_t period_ms = BOARD_TASK_BOOTSTRAP_PERIOD_MS;
#if APP_MOTOR_TEST_ENABLE
    (void)app_motor_test_start(&s_motor_test,
                               (bsp_kamoer_direction_t)APP_MOTOR_TEST_DIRECTION,
                               APP_MOTOR_TEST_SPEED_CENTI_RPM,
                               APP_MOTOR_TEST_DURATION_MS);
#endif
#endif
    osal_tick_type_t last_wake = osal_task_get_tick_count();
    for (;;)
    {
#if APP_DOSING_TEST_ENABLE
        if (active)
        {
            active = app_dosing_advance();
        }
#elif APP_DROP_COUNT_TEST_ENABLE
        if (sampling)
        {
            bsp_dosing_inputs_snapshot_t snapshot;
            fw_error_t err = s_drop_inputs.read_snapshot(s_drop_inputs.context, &snapshot);
            if (err != FW_OK)
            {
                fw_error_record_t error = {0};
                (void)core_pcnt_get_error(&error);
                printf("drop_count error=%s stage=%u native=%" PRId32 " cleanup=%s\n",
                       fw_error_code_name(err), (unsigned)error.stage,
                       error.native_status, fw_error_code_name(error.cleanup_code));
                sampling = false;
            }
            else if (!reported || snapshot.drop_count != last_count)
            {
                printf("drop_count: total=%" PRIu32 " delta=%" PRIu32 " t_ms=%" PRIu32 "\n",
                       snapshot.drop_count, snapshot.drop_count - last_count, snapshot.timestamp_ms);
                last_count = snapshot.drop_count;
                reported = true;
            }
        }
#else
        (void)app_motor_test_step(&s_motor_test);
#endif
        /* 短事务可能超过周期；超期后重新锚定，避免连续追赶。 */
        if (osal_task_elapsed_ms(osal_task_get_tick_count(), last_wake) >=
            period_ms)
        {
            last_wake = osal_task_get_tick_count();
        }
        if (osal_task_wait_until(&last_wake, period_ms) != OSAL_OK)
        {
#if APP_DOSING_TEST_ENABLE
            (void)service_dosing_cancel(&s_dosing);
#elif !APP_DROP_COUNT_TEST_ENABLE
            (void)app_motor_test_cancel(&s_motor_test);
#endif
            /* 保留句柄供维护清理；FreeRTOS任务入口不返回。 */
            (void)osal_task_suspend(NULL);
        }
    }
}

/** @brief 全工程唯一任务注册表，服务层与驱动层不得私自创建后台任务。 */
static const app_task_spec_t s_app_tasks[APP_TASK_COUNT] =
{
    [APP_TASK_BOOTSTRAP] = {
        .name = "app_bootstrap",
        .entry = bootstrap_task_entry,
        .stack_bytes = BOARD_TASK_BOOTSTRAP_STACK_BYTES,
        .priority = BOARD_TASK_BOOTSTRAP_PRIORITY,
        .handle = &s_bootstrap_task_handle
    }
};

/** @brief 统一记录应用层首个终态失败、保留底层源头并执行有界清理。 */
static fw_error_t app_fail_at(fw_error_t code, fw_error_t cause_code,
                              const fw_error_record_t *origin,
                              int32_t native_status, bool has_native,
                              fw_error_t cleanup_code,
                              app_stage_t stage,
                              const char *file, const char *function, uint32_t line)
{
    if (s_app_state == APP_STATE_FAULT && s_app_error.code != FW_OK)
    {
        return s_app_error.code;
    }
    fw_error_capture_at(&s_app_error, code, cause_code, (uint32_t)s_app_state,
                        (uint16_t)stage, 0U, osal_task_now_ms(),
                        file, function, line);
    if (origin != NULL && origin->code != FW_OK)
    {
        fw_error_set_origin(&s_app_error, origin);
    }
    else if (has_native)
    {
        s_app_error.native_status = native_status;
        s_app_error.native_status_valid = true;
    }
    s_app_error.cleanup_code = cleanup_code;
    s_app_state = APP_STATE_FAULT;
    return s_app_error.code;
}

/** @brief 在每个实际失败点捕获文件、函数和行号。 */
#define APP_FAIL(code, cause, origin, native_st, has_nat, cleanup, stage) \
    app_fail_at((code), (cause), (origin), (native_st), (has_nat), \
                (cleanup), (stage), __FILE__, __func__, (uint32_t)__LINE__)

/* 先释放采集/通信端口，再撤销GPIO配置。 */
static fw_error_t app_release_platform(void)
{
#if APP_DOSING_TEST_ENABLE || !APP_DROP_COUNT_TEST_ENABLE
    fw_error_t uart = core_uart_deinit(CORE_UART_ID_1);
#endif
#if APP_DOSING_TEST_ENABLE || APP_DROP_COUNT_TEST_ENABLE
    fw_error_t pcnt = core_pcnt_deinit();
    if (pcnt != FW_OK)
    {
        return pcnt;
    }
#endif
#if APP_DOSING_TEST_ENABLE || !APP_DROP_COUNT_TEST_ENABLE
    if (uart != FW_OK)
    {
        return uart;
    }
#endif
    return core_gpio_deinit();
}

fw_error_t app_init_platform(void)
{
    switch (s_app_state)
    {
        case APP_STATE_UNINITIALIZED:
            break;
        case APP_STATE_PLATFORM_READY:
        case APP_STATE_RUNNING:
            return FW_ERR_APP_ALREADY_INIT;
        case APP_STATE_FAULT:
            return s_app_error.code;
        default:
            return APP_FAIL(FW_ERR_APP_INVALID_STATE, FW_OK, NULL,
                            0, false, FW_OK, APP_STAGE_VALIDATE);
    }

    memset(&s_app_error, 0, sizeof(s_app_error));

    fw_error_t err = core_gpio_init();
    if (err != FW_OK)
    {
        core_gpio_state_t gpio_st = CORE_GPIO_STATE_UNINITIALIZED;
        fw_error_record_t gpio_rec = {0};
        (void)core_gpio_get_status(&gpio_st, &gpio_rec);
        return APP_FAIL(FW_ERR_APP_PLATFORM_INIT_FAILED, err, &gpio_rec,
                        0, false, FW_OK, APP_STAGE_GPIO_INIT);
    }

#if APP_DROP_COUNT_TEST_ENABLE || APP_DOSING_TEST_ENABLE
    err = core_pcnt_init();
    if (err != FW_OK)
    {
        fw_error_record_t record = {0};
        (void)core_pcnt_get_error(&record);
        fw_error_t cleanup = record.cleanup_code;
        if (cleanup == FW_OK)
        {
            cleanup = core_gpio_deinit();
        }
        return APP_FAIL(FW_ERR_APP_PLATFORM_INIT_FAILED, err, &record,
                        0, false, cleanup, APP_STAGE_DROP_COUNT_INIT);
    }
#endif
#if APP_DOSING_TEST_ENABLE || !APP_DROP_COUNT_TEST_ENABLE
    core_uart_config_t motor_uart;
    (void)core_uart_get_default_config(CORE_UART_ID_1, &motor_uart);
    motor_uart.baudrate = MOTOR_BAUDRATE;
    motor_uart.tx_buf_bytes = 0U; /* 短帧直接写FIFO，避免环形发送队列无界等待。 */
    motor_uart.mode = MOTOR_AUTO_DIRECTION ? CORE_UART_MODE_NORMAL :
                                            CORE_UART_MODE_RS485_HALF_DUPLEX;
    motor_uart.rts_de_pin = MOTOR_AUTO_DIRECTION ? BOARD_GPIO_PIN_NC :
                                                 BOARD_MOTOR_MANUAL_RTS_PIN;
    err = core_uart_init(CORE_UART_ID_1, &motor_uart);
    if (err != FW_OK)
    {
        core_uart_state_t uart_st = CORE_UART_STATE_UNINITIALIZED;
        fw_error_record_t uart_rec = {0};
        (void)core_uart_get_status(CORE_UART_ID_1, &uart_st, &uart_rec);
#if APP_DOSING_TEST_ENABLE
        fw_error_t clean_err = core_pcnt_deinit();
        if (clean_err == FW_OK)
        {
            clean_err = core_gpio_deinit();
        }
#else
        fw_error_t clean_err = core_gpio_deinit();
#endif
        return APP_FAIL(FW_ERR_APP_PLATFORM_INIT_FAILED, err, &uart_rec,
                        0, false, clean_err, APP_STAGE_UART_INIT);
    }

    /* 当前只启用电机UART1；UART2候选GPIO19/20保留给USB，不初始化。 */
#if APP_DOSING_TEST_ENABLE
    err = app_dosing_bind();
#else
    err = app_motor_test_init(&s_motor_test);
#endif
    if (err != FW_OK)
    {
        fw_error_t cleanup = app_release_platform();
#if APP_DOSING_TEST_ENABLE
        const fw_error_record_t *origin = &s_dosing_motor.error;
#else
        const fw_error_record_t *origin = &s_motor_test.error;
#endif
        return APP_FAIL(FW_ERR_APP_PLATFORM_INIT_FAILED, err, origin,
                        0, false, cleanup, APP_STAGE_MOTOR_BIND);
    }

#endif

    s_app_state = APP_STATE_PLATFORM_READY;
    return FW_OK;
}

fw_error_t app_init(void)
{
    if (s_app_state == APP_STATE_FAULT)
    {
        return s_app_error.code;
    }
    if (s_app_state == APP_STATE_RUNNING)
    {
        return FW_ERR_APP_ALREADY_INIT;
    }
    if (s_app_state == APP_STATE_UNINITIALIZED)
    {
        fw_error_t plat_err = app_init_platform();
        if (plat_err != FW_OK)
        {
            return plat_err;
        }
    }
    if (s_app_state != APP_STATE_PLATFORM_READY)
    {
        return APP_FAIL(FW_ERR_APP_INVALID_STATE, FW_OK, NULL,
                        0, false, FW_OK, APP_STAGE_VALIDATE);
    }

    for (size_t i = 0U; i < (size_t)APP_TASK_COUNT; ++i)
    {
        const app_task_spec_t *spec = &s_app_tasks[i];
        osal_status_t os_st = osal_task_create(spec->name,
                                               spec->entry,
                                               spec->stack_bytes,
                                               spec->priority,
                                               spec->handle,
                                               NULL);
        if (os_st != OSAL_OK)
        {
            fw_error_t cause = osal_status_to_fw_error(os_st);
            fw_error_t clean_err = app_release_platform();
            return APP_FAIL(FW_ERR_APP_TASK_CREATE_FAILED, cause, NULL,
                            (int32_t)os_st, true, clean_err, APP_STAGE_TASK_REGISTER);
        }
    }

    s_app_state = APP_STATE_RUNNING;
    return FW_OK;
}

fw_error_t app_deinit(void)
{
    /* 调用方须先停止bootstrap所有者；本接口不作为运行期跨任务急停入口。 */
    if (s_bootstrap_task_handle != NULL)
    {
        osal_status_t status = osal_task_delete(s_bootstrap_task_handle);
        if (status != OSAL_OK)
        {
            return osal_status_to_fw_error(status);
        }
        s_bootstrap_task_handle = NULL;
    }
    fw_error_t err = app_release_platform();
    if (err != FW_OK)
    {
        return err;
    }
    memset(&s_app_error, 0, sizeof(s_app_error));
    s_app_state = APP_STATE_UNINITIALIZED;
    return FW_OK;
}

fw_error_t app_get_status(app_state_t *state, fw_error_record_t *error)
{
    if (state == NULL || error == NULL)
    {
        return FW_ERR_APP_INVALID_ARG;
    }
    *state = s_app_state;
    *error = s_app_error;
    return FW_OK;
}
