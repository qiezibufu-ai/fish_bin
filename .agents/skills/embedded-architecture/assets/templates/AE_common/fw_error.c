/** @file fw_error.c @brief 实现统一错误注册表和错误快照。 */
#include "fw_error.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    fw_error_t code;
    const char *name;
    const char *text_zh;
} error_entry_t;

/* 每个新终态错误必须在此登记，重复code会由模板测试发现。 */
static const error_entry_t s_entries[] =
{
    {FW_OK, "FW_OK", "操作成功"},
    {FW_ERR_PORT_TIMEOUT, "FW_ERR_PORT_TIMEOUT", "平台总线事务超时"},
    {FW_ERR_PORT_BUSY, "FW_ERR_PORT_BUSY", "平台总线未接受事务"},
    {FW_ERR_PORT_IO, "FW_ERR_PORT_IO", "平台总线传输错误"},
    {FW_ERR_SENSOR_INVALID_ARG, "FW_ERR_SENSOR_INVALID_ARG", "传感器参数无效"},
    {FW_ERR_SENSOR_NOT_READY, "FW_ERR_SENSOR_NOT_READY", "传感器或样本尚未就绪"},
    {FW_ERR_SENSOR_BUSY, "FW_ERR_SENSOR_BUSY", "传感器已有活动请求"},
    {FW_ERR_SENSOR_ID_READ_FAILED, "FW_ERR_SENSOR_ID_READ_FAILED", "初始化读取设备ID失败"},
    {FW_ERR_SENSOR_ID_MISMATCH, "FW_ERR_SENSOR_ID_MISMATCH", "设备ID与预期不符"},
    {FW_ERR_SENSOR_TRIGGER_UNKNOWN, "FW_ERR_SENSOR_TRIGGER_UNKNOWN", "触发写失败且执行结果未知"},
    {FW_ERR_SENSOR_READ_FAILED, "FW_ERR_SENSOR_READ_FAILED", "样本读取事务失败"},
    {FW_ERR_SENSOR_SAMPLE_TIMEOUT, "FW_ERR_SENSOR_SAMPLE_TIMEOUT", "采样总时间预算耗尽"},
    {FW_ERR_SENSOR_CRC, "FW_ERR_SENSOR_CRC", "样本校验失败"},
    {FW_ERR_SENSOR_INVALID_STATE, "FW_ERR_SENSOR_INVALID_STATE", "传感器状态值未定义"}
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
