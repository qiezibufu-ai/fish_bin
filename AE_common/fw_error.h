/**
 * @file fw_error.h
 * @brief 定义工程统一错误码、模块注册表、中文解释与可定位的错误快照。
 */
#ifndef AE_COMMON_FW_ERROR_H
#define AE_COMMON_FW_ERROR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 稳定错误身份，格式为 0xLLMMRRRR（层ID/模块ID/16位原因ID）。 */
typedef uint32_t fw_error_t;

/** @brief 操作成功，不代表异步动作已经完成。 */
#define FW_OK UINT32_C(0)

/* AB_service: 滴定服务 (0x0201)。 */
/** @brief 请求参数无效。 */
#define FW_ERR_DOSING_ARG UINT32_C(0x02010001)
/** @brief 滴定服务未绑定。 */
#define FW_ERR_DOSING_NOT_READY UINT32_C(0x02010002)
/** @brief 已有滴定请求。 */
#define FW_ERR_DOSING_BUSY UINT32_C(0x02010003)
/** @brief 电机配置失败。 */
#define FW_ERR_DOSING_SETUP UINT32_C(0x02010004)
/** @brief 启动未确认。 */
#define FW_ERR_DOSING_START UINT32_C(0x02010005)
/** @brief 停车未确认。 */
#define FW_ERR_DOSING_STOP UINT32_C(0x02010006)
/** @brief 计数无效或回退。 */
#define FW_ERR_DOSING_COUNT UINT32_C(0x02010007)
/** @brief 无滴超时。 */
#define FW_ERR_DOSING_NO_DROP UINT32_C(0x02010008)
/** @brief 泵运行超时。 */
#define FW_ERR_DOSING_RUN_TIMEOUT UINT32_C(0x02010009)
/** @brief 尾滴观察超时。 */
#define FW_ERR_DOSING_TAIL_TIMEOUT UINT32_C(0x0201000A)
/** @brief 滴定阶段无效。 */
#define FW_ERR_DOSING_STATE UINT32_C(0x0201000B)

/* ==================== AD_platform: 通用总线端口 (0x0401) ==================== */
/** @brief 平台总线01的事务超时。 */
#define FW_ERR_PORT_TIMEOUT UINT32_C(0x04010001)
/** @brief 平台总线01未接受事务（资源忙）。 */
#define FW_ERR_PORT_BUSY UINT32_C(0x04010002)
/** @brief 平台总线01报告传输错误。 */
#define FW_ERR_PORT_IO UINT32_C(0x04010003)
/** @brief 平台总线01收到非法参数。 */
#define FW_ERR_PORT_INVALID_ARG UINT32_C(0x04010004)
/** @brief 平台总线01尚未初始化。 */
#define FW_ERR_PORT_NOT_READY UINT32_C(0x04010005)
/** @brief 平台总线01内存或驱动资源不足。 */
#define FW_ERR_PORT_NO_RESOURCE UINT32_C(0x04010006)
/** @brief 平台总线01不支持当前请求的操作或上下文。 */
#define FW_ERR_PORT_NOT_SUPPORTED UINT32_C(0x04010007)

/* ==================== AD_platform: OSAL 抽象层 (0x0402) ===================== */
/** @brief OSAL 收到非法参数。 */
#define FW_ERR_OSAL_INVALID_ARG UINT32_C(0x04020001)
/** @brief OSAL 等待超时。 */
#define FW_ERR_OSAL_TIMEOUT UINT32_C(0x04020002)
/** @brief OSAL 动态内存或内核对象不足。 */
#define FW_ERR_OSAL_NO_MEMORY UINT32_C(0x04020003)
/** @brief OSAL 队列已满或资源忙。 */
#define FW_ERR_OSAL_BUSY UINT32_C(0x04020004)
/** @brief OSAL 当前上下文（如 ISR）不支持该操作。 */
#define FW_ERR_OSAL_NOT_SUPPORTED UINT32_C(0x04020005)
/** @brief OSAL 资源状态非法或所有权不匹配。 */
#define FW_ERR_OSAL_INVALID_STATE UINT32_C(0x04020006)
/** @brief OSAL 底层内核操作失败。 */
#define FW_ERR_OSAL_FAIL UINT32_C(0x04020007)

/* ==================== AD_platform: GPIO 端口 (0x0403) ======================= */
/** @brief GPIO 端口参数无效或引脚编号越界。 */
#define FW_ERR_GPIO_INVALID_ARG UINT32_C(0x04030001)
/** @brief GPIO 端口尚未完成初始化。 */
#define FW_ERR_GPIO_NOT_READY UINT32_C(0x04030002)
/** @brief GPIO 底层引脚配置失败。 */
#define FW_ERR_GPIO_CONFIG_FAILED UINT32_C(0x04030003)
/** @brief GPIO 读写电平或方向切换失败。 */
#define FW_ERR_GPIO_IO_FAILED UINT32_C(0x04030004)
/** @brief GPIO 中断服务安装或回调注册失败。 */
#define FW_ERR_GPIO_ISR_FAILED UINT32_C(0x04030005)
/** @brief GPIO 端口进入未定义内部状态。 */
#define FW_ERR_GPIO_INVALID_STATE UINT32_C(0x04030006)

/* ==================== AD_platform: UART 端口 (0x0404) ======================= */
/** @brief UART 端口参数无效或实例越界。 */
#define FW_ERR_UART_INVALID_ARG UINT32_C(0x04040001)
/** @brief UART 端口尚未初始化或未就绪。 */
#define FW_ERR_UART_NOT_READY UINT32_C(0x04040002)
/** @brief UART 端口正在执行冲突操作或缓冲区忙。 */
#define FW_ERR_UART_BUSY UINT32_C(0x04040003)
/** @brief UART 端口收发等待超时。 */
#define FW_ERR_UART_TIMEOUT UINT32_C(0x04040004)
/** @brief UART 底层参数、引脚或模式配置失败。 */
#define FW_ERR_UART_CONFIG_FAILED UINT32_C(0x04040005)
/** @brief UART 驱动安装或卸载失败。 */
#define FW_ERR_UART_DRIVER_FAILED UINT32_C(0x04040006)
/** @brief UART 发送事务失败。 */
#define FW_ERR_UART_TX_FAILED UINT32_C(0x04040007)
/** @brief UART 接收事务或硬件FIFO溢出失败。 */
#define FW_ERR_UART_RX_FAILED UINT32_C(0x04040008)
/** @brief UART 端口进入未定义状态。 */
#define FW_ERR_UART_INVALID_STATE UINT32_C(0x04040009)

/* AD_platform: PCNT液滴采集端口 (0x0405)。 */
/** @brief PCNT资源或采集配置初始化失败。 */
#define FW_ERR_PCNT_INIT UINT32_C(0x04050001)
/** @brief PCNT读取失败或累计值超出台架范围。 */
#define FW_ERR_PCNT_READ UINT32_C(0x04050002)
/** @brief PCNT资源释放失败。 */
#define FW_ERR_PCNT_RELEASE UINT32_C(0x04050003)
/** @brief PCNT端口内部生命周期状态未定义。 */
#define FW_ERR_PCNT_INVALID_STATE UINT32_C(0x04050004)

/* ==================== AE_common: 环形缓冲区 (0x0501) ======================== */
/** @brief 环形缓冲区参数非法或容量为0。 */
#define FW_ERR_RINGBUF_INVALID_ARG UINT32_C(0x05010001)
/** @brief 环形缓冲区空间不足，无法写入全部数据。 */
#define FW_ERR_RINGBUF_FULL UINT32_C(0x05010002)
/** @brief 环形缓冲区无足够可读数据。 */
#define FW_ERR_RINGBUF_EMPTY UINT32_C(0x05010003)

/* ==================== AC_module: 示例传感器驱动 (0x0310) ==================== */
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

/* AC_module: Kamoer 4460.5 (0x0311)。 */
/** @brief 参数无效。 */
#define FW_ERR_KAMOER_INVALID_ARG UINT32_C(0x03110001)
/** @brief 设备尚未绑定。 */
#define FW_ERR_KAMOER_NOT_READY UINT32_C(0x03110002)
/** @brief 设备已有活动请求。 */
#define FW_ERR_KAMOER_BUSY UINT32_C(0x03110003)
/** @brief 电机总线事务失败。 */
#define FW_ERR_KAMOER_IO UINT32_C(0x03110004)
/** @brief 电机读取应答超时。 */
#define FW_ERR_KAMOER_TIMEOUT UINT32_C(0x03110005)
/** @brief 电机应答CRC错误。 */
#define FW_ERR_KAMOER_CRC UINT32_C(0x03110006)
/** @brief 电机应答格式或回显不匹配。 */
#define FW_ERR_KAMOER_RESPONSE UINT32_C(0x03110007)
/** @brief 电机返回Modbus异常。 */
#define FW_ERR_KAMOER_EXCEPTION UINT32_C(0x03110008)
/** @brief 写命令结果未知，禁止自动重发。 */
#define FW_ERR_KAMOER_EXECUTION_UNKNOWN UINT32_C(0x03110009)
/** @brief 电机内部状态未定义。 */
#define FW_ERR_KAMOER_INVALID_STATE UINT32_C(0x0311000A)

/* ==================== AA_app: 应用组合根 (0x0101) =========================== */
/** @brief 应用层收到非法参数。 */
#define FW_ERR_APP_INVALID_ARG UINT32_C(0x01010001)
/** @brief 应用层尚未初始化。 */
#define FW_ERR_APP_NOT_READY UINT32_C(0x01010002)
/** @brief 应用层重复初始化或任务已注册。 */
#define FW_ERR_APP_ALREADY_INIT UINT32_C(0x01010003)
/** @brief 应用层平台端口初始化失败。 */
#define FW_ERR_APP_PLATFORM_INIT_FAILED UINT32_C(0x01010004)
/** @brief 应用层任务注册创建失败。 */
#define FW_ERR_APP_TASK_CREATE_FAILED UINT32_C(0x01010005)
/** @brief 应用层进入未定义状态。 */
#define FW_ERR_APP_INVALID_STATE UINT32_C(0x01010006)

/** @brief 应用电机台架测试失败，cause_code保留设备/端口原因。 */
#define FW_ERR_APP_MOTOR_TEST_FAILED UINT32_C(0x01020001)
/** @brief 应用电机台架FSM状态非法。 */
#define FW_ERR_APP_MOTOR_INVALID_STATE UINT32_C(0x01020002)

/** @brief 故障清理后仍可读取的首个失败快照，调用者负责同步。 */
typedef struct
{
    fw_error_t code;              /**< 本模块终态原因。 */
    fw_error_t cause_code;        /**< 直接底层错误，无底层原因时为FW_OK。 */
    fw_error_t cleanup_code;      /**< 清理失败码，成功或未执行清理时为FW_OK。 */
    int32_t native_status;        /**< 原生HAL/ESP-IDF/OS/设备状态，须结合有效标志。 */
    bool native_status_valid;     /**< 是否已捕获原生状态。 */
    uint32_t state;               /**< 失败前状态值，保留非法枚举的原值。 */
    uint16_t stage;               /**< 失败所在操作阶段。 */
    uint32_t request_id;          /**< 所属请求，未分配请求时为0。 */
    uint32_t timestamp_ms;        /**< 失败时本地单调毫秒值，允许回绕。 */
    const char *file;             /**< 本层失败点文件字符串，借用固件静态存储。 */
    const char *function;         /**< 本层失败点函数字符串，借用固件静态存储。 */
    uint32_t line;                /**< 本层失败点行号。 */
    const char *origin_file;      /**< 已有底层诊断的最初文件，无记录时为NULL。 */
    const char *origin_function;  /**< 已有底层诊断的最初函数。 */
    uint32_t origin_line;         /**< 已有底层诊断的最初行号。 */
} fw_error_record_t;

/**
 * @brief 查询错误码的稳定符号名称。
 * @param[in] code 待解释的完整错误码。
 * @return 借用的只读静态字符串；未注册时返回"FW_ERR_UNKNOWN"。
 * @note 不阻塞、无共享可变状态，可在任务或ISR上下文调用；不进行格式化IO。
 */
const char *fw_error_code_name(fw_error_t code);

/**
 * @brief 查询错误码的中文说明。
 * @param[in] code 待解释的完整错误码。
 * @return 借用的UTF-8静态字符串；未注册时返回"未注册错误码"。
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
#endif /* AE_COMMON_FW_ERROR_H */
