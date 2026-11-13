/**
 * @file fw_error.h
 * @brief 定义工程统一错误码、中文解释与可定位的错误快照。
 */
#ifndef AE_COMMON_FW_ERROR_H
#define AE_COMMON_FW_ERROR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 稳定错误身份，格式为0x层ID/模块ID/16位原因ID。 */
typedef uint32_t fw_error_t;

/** @brief 操作成功，不代表异步动作已经完成。 */
#define FW_OK UINT32_C(0)
/** @brief 平台总线01的事务超时。 */
#define FW_ERR_PORT_TIMEOUT UINT32_C(0x04010001)
/** @brief 平台总线01未接受事务。 */
#define FW_ERR_PORT_BUSY UINT32_C(0x04010002)
/** @brief 平台总线01报告传输错误。 */
#define FW_ERR_PORT_IO UINT32_C(0x04010003)
/** @brief 示例传感器模块10收到非法参数。 */
#define FW_ERR_SENSOR_INVALID_ARG UINT32_C(0x03100001)
/** @brief 示例传感器尚未初始化或没有完成样本。 */
#define FW_ERR_SENSOR_NOT_READY UINT32_C(0x03100002)
/** @brief 示例传感器已有活动测量，未接受新请求。 */
#define FW_ERR_SENSOR_BUSY UINT32_C(0x03100003)
/** @brief 示例传感器初始化读取设备ID失败。 */
#define FW_ERR_SENSOR_ID_READ_FAILED UINT32_C(0x03100004)
/** @brief 示例传感器设备ID与预期不符。 */
#define FW_ERR_SENSOR_ID_MISMATCH UINT32_C(0x03100005)
/** @brief 示例传感器触发写失败，转换是否已启动未知。 */
#define FW_ERR_SENSOR_TRIGGER_UNKNOWN UINT32_C(0x03100006)
/** @brief 示例传感器样本读事务失败。 */
#define FW_ERR_SENSOR_READ_FAILED UINT32_C(0x03100007)
/** @brief 示例传感器整次采样预算耗尽。 */
#define FW_ERR_SENSOR_SAMPLE_TIMEOUT UINT32_C(0x03100008)
/** @brief 示例传感器样本校验失败。 */
#define FW_ERR_SENSOR_CRC UINT32_C(0x03100009)
/** @brief 示例传感器进入未定义状态。 */
#define FW_ERR_SENSOR_INVALID_STATE UINT32_C(0x0310000A)

/** @brief 故障清理后仍可读取的首个失败快照，调用者负责同步。 */
typedef struct
{
    fw_error_t code;                 /**< 本模块终态原因。 */
    fw_error_t cause_code;           /**< 直接底层错误，无底层原因时为FW_OK。 */
    fw_error_t cleanup_code;         /**< 清理失败码，成功或未执行清理时为FW_OK。 */
    int32_t native_status;           /**< 原生HAL/OS/设备状态，须结合有效标志。 */
    bool native_status_valid;       /**< 是否已捕获原生状态。 */
    uint32_t state;                 /**< 失败前状态值，保留非法枚举的原值。 */
    uint16_t stage;                 /**< 失败所在操作阶段。 */
    uint32_t request_id;            /**< 所属请求，未分配请求时为0。 */
    uint32_t timestamp_ms;          /**< 失败时本地单调毫秒值，允许回绕。 */
    const char *file;               /**< 本层失败点文件字符串，借用固件静态存储。 */
    const char *function;           /**< 本层失败点函数字符串，借用固件静态存储。 */
    uint32_t line;                  /**< 本层失败点行号。 */
    const char *origin_file;        /**< 已有底层诊断的最初文件，无记录时为NULL。 */
    const char *origin_function;    /**< 已有底层诊断的最初函数。 */
    uint32_t origin_line;           /**< 已有底层诊断的最初行号。 */
} fw_error_record_t;

/**
 * @brief 查询错误码的稳定符号名称。
 * @param[in] code 待解释的完整错误码。
 * @return 借用的只读静态字符串；未注册时返回FW_ERR_UNKNOWN。
 * @note 不阻塞、无共享可变状态，可在任务上下文调用；不进行格式化IO。
 */
const char *fw_error_code_name(fw_error_t code);

/**
 * @brief 查询错误码的中文说明。
 * @param[in] code 待解释的完整错误码。
 * @return 借用的UTF-8静态字符串；未注册时返回“未注册错误码”。
 * @note 不阻塞，无动态分配；调用者记录未知码时还须保留原始数值。
 */
const char *fw_error_code_text_zh(fw_error_t code);

/**
 * @brief 初始化一份失败快照，不执行日志或设备操作。
 * @param[out] record 接收完整记录；为NULL时不操作。
 * @param[in] code 本层终态错误，不得使用FW_OK描述失败。
 * @param[in] cause_code 直接底层错误，或FW_OK。
 * @param[in] state 失败前状态值。
 * @param[in] stage 失败操作阶段。
 * @param[in] request_id 所属请求ID，初始化阶段可为0。
 * @param[in] timestamp_ms 本地失败时间，单位ms。
 * @param[in] file 失败点静态文件字符串，寿命覆盖本记录。
 * @param[in] function 失败点静态函数字符串，寿命覆盖本记录。
 * @param[in] line 失败点源码行号。
 * @note 同一record的并发访问由调用方保护；调用本函数会替换旧记录。
 */
void fw_error_capture_at(fw_error_record_t *record, fw_error_t code,
                         fw_error_t cause_code, uint32_t state, uint16_t stage,
                         uint32_t request_id, uint32_t timestamp_ms,
                         const char *file, const char *function, uint32_t line);

/**
 * @brief 包装底层失败时复制最初失败位置与原生状态。
 * @param[in,out] record 已保存本层失败点的记录，不得为NULL。
 * @param[in] cause 底层已记录的失败，不能与record指向同一对象；NULL时不操作。
 * @note 不改变本层code、file、function、line及cleanup_code；不保存cause指针。
 */
void fw_error_set_origin(fw_error_record_t *record, const fw_error_record_t *cause);

#ifdef __cplusplus
}
#endif
#endif
