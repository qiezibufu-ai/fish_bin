/**
 * @file fw_error.c
 * @brief 实现统一错误注册表、中文描述查询与可定位错误快照。
 */
#include "fw_error.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    fw_error_t code;
    const char *name;
    const char *text_zh;
} error_entry_t;

/* 全工程唯一错误注册表，每个终态错误均在此登记，禁止重复数值。 */
static const error_entry_t s_entries[] =
{
    {FW_OK, "FW_OK", "操作成功"},

    /* AD_platform: 通用总线端口 (0x0401) */
    {FW_ERR_PORT_TIMEOUT, "FW_ERR_PORT_TIMEOUT", "平台总线事务超时"},
    {FW_ERR_PORT_BUSY, "FW_ERR_PORT_BUSY", "平台总线未接受事务"},
    {FW_ERR_PORT_IO, "FW_ERR_PORT_IO", "平台总线传输错误"},
    {FW_ERR_PORT_INVALID_ARG, "FW_ERR_PORT_INVALID_ARG", "平台总线参数无效"},
    {FW_ERR_PORT_NOT_READY, "FW_ERR_PORT_NOT_READY", "平台总线尚未初始化"},
    {FW_ERR_PORT_NO_RESOURCE, "FW_ERR_PORT_NO_RESOURCE", "平台总线资源不足"},
    {FW_ERR_PORT_NOT_SUPPORTED, "FW_ERR_PORT_NOT_SUPPORTED", "平台总线不支持该操作"},

    /* AD_platform: OSAL 抽象层 (0x0402) */
    {FW_ERR_OSAL_INVALID_ARG, "FW_ERR_OSAL_INVALID_ARG", "OSAL参数无效"},
    {FW_ERR_OSAL_TIMEOUT, "FW_ERR_OSAL_TIMEOUT", "OSAL等待超时"},
    {FW_ERR_OSAL_NO_MEMORY, "FW_ERR_OSAL_NO_MEMORY", "OSAL内存或内核资源不足"},
    {FW_ERR_OSAL_BUSY, "FW_ERR_OSAL_BUSY", "OSAL队列满或资源忙"},
    {FW_ERR_OSAL_NOT_SUPPORTED, "FW_ERR_OSAL_NOT_SUPPORTED", "OSAL当前上下文不支持该操作"},
    {FW_ERR_OSAL_INVALID_STATE, "FW_ERR_OSAL_INVALID_STATE", "OSAL资源状态非法"},
    {FW_ERR_OSAL_FAIL, "FW_ERR_OSAL_FAIL", "OSAL内核调用失败"},

    /* AD_platform: GPIO 端口 (0x0403) */
    {FW_ERR_GPIO_INVALID_ARG, "FW_ERR_GPIO_INVALID_ARG", "GPIO参数无效或引脚越界"},
    {FW_ERR_GPIO_NOT_READY, "FW_ERR_GPIO_NOT_READY", "GPIO端口尚未初始化"},
    {FW_ERR_GPIO_CONFIG_FAILED, "FW_ERR_GPIO_CONFIG_FAILED", "GPIO硬件配置失败"},
    {FW_ERR_GPIO_IO_FAILED, "FW_ERR_GPIO_IO_FAILED", "GPIO读写或方向控制失败"},
    {FW_ERR_GPIO_ISR_FAILED, "FW_ERR_GPIO_ISR_FAILED", "GPIO中断服务或回调设置失败"},
    {FW_ERR_GPIO_INVALID_STATE, "FW_ERR_GPIO_INVALID_STATE", "GPIO内部状态未定义"},

    /* AD_platform: UART 端口 (0x0404) */
    {FW_ERR_UART_INVALID_ARG, "FW_ERR_UART_INVALID_ARG", "UART参数无效或实例越界"},
    {FW_ERR_UART_NOT_READY, "FW_ERR_UART_NOT_READY", "UART端口尚未初始化"},
    {FW_ERR_UART_BUSY, "FW_ERR_UART_BUSY", "UART端口忙"},
    {FW_ERR_UART_TIMEOUT, "FW_ERR_UART_TIMEOUT", "UART收发事务超时"},
    {FW_ERR_UART_CONFIG_FAILED, "FW_ERR_UART_CONFIG_FAILED", "UART底层配置失败"},
    {FW_ERR_UART_DRIVER_FAILED, "FW_ERR_UART_DRIVER_FAILED", "UART驱动安装或卸载失败"},
    {FW_ERR_UART_TX_FAILED, "FW_ERR_UART_TX_FAILED", "UART发送事务失败"},
    {FW_ERR_UART_RX_FAILED, "FW_ERR_UART_RX_FAILED", "UART接收事务失败"},
    {FW_ERR_UART_INVALID_STATE, "FW_ERR_UART_INVALID_STATE", "UART内部状态未定义"},

    /* AE_common: 环形缓冲区 (0x0501) */
    {FW_ERR_RINGBUF_INVALID_ARG, "FW_ERR_RINGBUF_INVALID_ARG", "环形缓冲区参数无效"},
    {FW_ERR_RINGBUF_FULL, "FW_ERR_RINGBUF_FULL", "环形缓冲区已满"},
    {FW_ERR_RINGBUF_EMPTY, "FW_ERR_RINGBUF_EMPTY", "环形缓冲区为空或数据不足"},

    /* AC_module: 示例传感器驱动 (0x0310) */
    {FW_ERR_SENSOR_INVALID_ARG, "FW_ERR_SENSOR_INVALID_ARG", "传感器参数无效"},
    {FW_ERR_SENSOR_NOT_READY, "FW_ERR_SENSOR_NOT_READY", "传感器或样本尚未就绪"},
    {FW_ERR_SENSOR_BUSY, "FW_ERR_SENSOR_BUSY", "传感器已有活动请求"},
    {FW_ERR_SENSOR_ID_READ_FAILED, "FW_ERR_SENSOR_ID_READ_FAILED", "初始化读取设备ID失败"},
    {FW_ERR_SENSOR_ID_MISMATCH, "FW_ERR_SENSOR_ID_MISMATCH", "设备ID与预期不符"},
    {FW_ERR_SENSOR_TRIGGER_UNKNOWN, "FW_ERR_SENSOR_TRIGGER_UNKNOWN", "触发写失败且执行结果未知"},
    {FW_ERR_SENSOR_READ_FAILED, "FW_ERR_SENSOR_READ_FAILED", "样本读取事务失败"},
    {FW_ERR_SENSOR_SAMPLE_TIMEOUT, "FW_ERR_SENSOR_SAMPLE_TIMEOUT", "采样总时间预算耗尽"},
    {FW_ERR_SENSOR_CRC, "FW_ERR_SENSOR_CRC", "样本校验失败"},
    {FW_ERR_SENSOR_INVALID_STATE, "FW_ERR_SENSOR_INVALID_STATE", "传感器状态值未定义"},

    {FW_ERR_KAMOER_INVALID_ARG, "FW_ERR_KAMOER_INVALID_ARG", "参数无效"},
    {FW_ERR_KAMOER_NOT_READY, "FW_ERR_KAMOER_NOT_READY", "设备尚未绑定"},
    {FW_ERR_KAMOER_BUSY, "FW_ERR_KAMOER_BUSY", "设备已有活动请求"},
    {FW_ERR_KAMOER_IO, "FW_ERR_KAMOER_IO", "电机总线事务失败"},
    {FW_ERR_KAMOER_TIMEOUT, "FW_ERR_KAMOER_TIMEOUT", "电机读取应答超时"},
    {FW_ERR_KAMOER_CRC, "FW_ERR_KAMOER_CRC", "电机应答CRC错误"},
    {FW_ERR_KAMOER_RESPONSE, "FW_ERR_KAMOER_RESPONSE", "电机应答格式或回显不匹配"},
    {FW_ERR_KAMOER_EXCEPTION, "FW_ERR_KAMOER_EXCEPTION", "电机返回Modbus异常"},
    {FW_ERR_KAMOER_EXECUTION_UNKNOWN, "FW_ERR_KAMOER_EXECUTION_UNKNOWN", "写命令结果未知，禁止自动重发"},
    {FW_ERR_KAMOER_INVALID_STATE, "FW_ERR_KAMOER_INVALID_STATE", "电机内部状态未定义"},

    {FW_ERR_APP_MOTOR_TEST_FAILED, "FW_ERR_APP_MOTOR_TEST_FAILED", "应用电机台架测试失败"},
    {FW_ERR_APP_MOTOR_INVALID_STATE, "FW_ERR_APP_MOTOR_INVALID_STATE", "应用电机台架状态未定义"},

    /* AA_app: 应用组合根 (0x0101) */
    {FW_ERR_APP_INVALID_ARG, "FW_ERR_APP_INVALID_ARG", "应用层参数无效"},
    {FW_ERR_APP_NOT_READY, "FW_ERR_APP_NOT_READY", "应用层尚未初始化"},
    {FW_ERR_APP_ALREADY_INIT, "FW_ERR_APP_ALREADY_INIT", "应用层重复初始化"},
    {FW_ERR_APP_PLATFORM_INIT_FAILED, "FW_ERR_APP_PLATFORM_INIT_FAILED", "应用层平台端口初始化失败"},
    {FW_ERR_APP_TASK_CREATE_FAILED, "FW_ERR_APP_TASK_CREATE_FAILED", "应用层任务注册创建失败"},
    {FW_ERR_APP_INVALID_STATE, "FW_ERR_APP_INVALID_STATE", "应用层状态值未定义"}
};

const char *fw_error_code_name(fw_error_t code)
{
    for (size_t i = 0U; i < sizeof(s_entries) / sizeof(s_entries[0]); ++i)
    {
        if (s_entries[i].code == code)
        {
            return s_entries[i].name;
        }
    }
    return "FW_ERR_UNKNOWN";
}

const char *fw_error_code_text_zh(fw_error_t code)
{
    for (size_t i = 0U; i < sizeof(s_entries) / sizeof(s_entries[0]); ++i)
    {
        if (s_entries[i].code == code)
        {
            return s_entries[i].text_zh;
        }
    }
    return "未注册错误码";
}

void fw_error_capture_at(fw_error_record_t *record, fw_error_t code,
                         fw_error_t cause_code, uint32_t state, uint16_t stage,
                         uint32_t request_id, uint32_t timestamp_ms,
                         const char *file, const char *function, uint32_t line)
{
    if (record == NULL)
    {
        return;
    }
    memset(record, 0, sizeof(*record));
    record->code = code;
    record->cause_code = cause_code;
    record->state = state;
    record->stage = stage;
    record->request_id = request_id;
    record->timestamp_ms = timestamp_ms;
    record->file = file;
    record->function = function;
    record->line = line;
}

void fw_error_set_origin(fw_error_record_t *record, const fw_error_record_t *cause)
{
    if (record == NULL || cause == NULL || record == cause)
    {
        return;
    }
    record->cause_code = cause->code;
    record->origin_file = cause->origin_file != NULL ? cause->origin_file : cause->file;
    record->origin_function = cause->origin_function != NULL ?
                              cause->origin_function : cause->function;
    record->origin_line = cause->origin_file != NULL ? cause->origin_line : cause->line;
    record->native_status = cause->native_status;
    record->native_status_valid = cause->native_status_valid;
}
