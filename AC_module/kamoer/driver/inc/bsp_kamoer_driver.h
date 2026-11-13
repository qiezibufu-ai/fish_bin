/** @file bsp_kamoer_driver.h @brief Kamoer 4460.5 Modbus RTU电机协议；不依赖MCU/OS。 */
#ifndef AC_MODULE_KAMOER_DRIVER_INC_BSP_KAMOER_DRIVER_H
#define AC_MODULE_KAMOER_DRIVER_INC_BSP_KAMOER_DRIVER_H
#include "fw_error.h"
#include <stddef.h>
#include <stdint.h>

/** @brief 电机事务端口，context及回调的寿命覆盖实例；同一总线由调用者串行化。 */
typedef struct
{
    /**
     * @brief 同步收发一帧；负责发送前RTU静默间隔和清理旧帧，不重发。
     * @param[in] context 外部总线上下文，可为NULL。
     * @param[in] tx 请求帧，只在调用期间有效。
     * @param[in] tx_bytes 请求字节数，8或13。
     * @param[out] rx 响应缓冲；内容仅在返回FW_OK时有效。
     * @param[in] capacity 响应容量，至少21字节。
     * @param[out] rx_bytes 响应长度；失败为0。
     * @param[in] timeout_ms 整个收发事务预算，1..2000ms。
     * @param[out] origin 底层诊断；没有记录时清零。
     * @retval FW_OK 已收到完整帧（不代表CRC或内容正确）。
     * @return 端口的具体错误；可能部分发送，写请求不得自动重发。
     * @note 任务上下文；阻塞不超过timeout_ms加一个OS节拍；返回后可复用缓冲。
     */
    fw_error_t (*exchange)(void *context, const uint8_t *tx, size_t tx_bytes,
                           uint8_t *rx, size_t capacity, size_t *rx_bytes,
                           uint32_t timeout_ms, fw_error_record_t *origin);
    /**
     * @brief 获取单调毫秒时间，允许32位回绕。
     * @param[in] context 与exchange共用的上下文。
     * @return 当前毫秒时间戳。
     * @note 不阻塞；不得返回墙钟时间。
     */
    uint32_t (*now_ms)(void *context);
    void *context; /**< 外部所有的端口上下文。 */
} bsp_kamoer_ops_t;

/** @brief 驱动通信生命周期，不表示电机物理运行状态。 */
typedef enum
{
    BSP_KAMOER_UNINITIALIZED = 0, /**< 未绑定。 */
    BSP_KAMOER_READY,             /**< 可以提交事务。 */
    BSP_KAMOER_FAULT              /**< 首错锁存；只允许显式STOP。 */
} bsp_kamoer_state_t;
/** @brief 电机转向；实际出液方向须接管后确认。 */
typedef enum
{
    BSP_KAMOER_CW = 0, /**< 顺时针，线圈值0x0000。 */
    BSP_KAMOER_CCW = 1 /**< 逆时针，线圈值0xFF00。 */
} bsp_kamoer_direction_t;
/** @brief 输入寄存器快照；脉冲不是编码器或液滴计数。 */
typedef struct
{
    uint32_t speed_centi_rpm; /**< 驱动板报告速度，0.01rpm。 */
    uint32_t position_pulse;  /**< 本次触发运行后的位置增量，pulse。 */
    uint16_t firmware;        /**< 原始固件版本值。 */
    uint16_t hardware;        /**< 原始硬件版本值。 */
    uint32_t serial;          /**< 设备序列号。 */
    uint32_t timestamp_ms;    /**< 本次完整响应接收时间。 */
} bsp_kamoer_feedback_t;
/** @brief 调用者静态分配并清零的实例；字段仅由驱动维护。 */
typedef struct
{
    bsp_kamoer_ops_t ops;       /**< 已绑定端口。 */
    bsp_kamoer_state_t state;   /**< 通信生命周期。 */
    fw_error_record_t error;    /**< 首个失败及独立STOP清理错误。 */
    uint32_t request_id;        /**< 递增事务ID，允许回绕。 */
    uint32_t timeout_ms;        /**< 单事务总预算，1..2000ms。 */
    uint8_t address;            /**< 从站地址1..247；禁止广播运动。 */
} bsp_kamoer_t;

/**
 * @brief 绑定实例，不访问总线、不改变电机状态；显式恢复须先完成硬件对账及STOP。
 * @param[out] self 清零的新实例或已停机且串口恢复的实例，不得为NULL。
 * @param[in] ops 有效回调，不得为NULL；复制保存。
 * @param[in] address 从站1..247。
 * @param[in] timeout_ms 单事务1..2000ms。
 * @retval FW_OK 已就绪。
 * @retval FW_ERR_KAMOER_INVALID_ARG 参数非法，实例不变。
 * @note 不阻塞；任务上下文；同实例所有API及快照访问必须由一个任务串行执行。
 */
fw_error_t bsp_kamoer_init(bsp_kamoer_t *self, const bsp_kamoer_ops_t *ops,
                          uint8_t address, uint32_t timeout_ms);
/**
 * @brief 设置转向，只确认设备ACK，不确认物理转向。
 * @param[in,out] self 已绑定实例，不得为NULL。
 * @param[in] direction 顺/逆时针枚举；调用方应先停车。
 * @retval FW_OK 设备ACK有效。
 * @retval FW_ERR_KAMOER_INVALID_ARG 参数无效。
 * @return 见下述公共事务错误契约。
 * @note 任务上下文；阻塞至多timeout_ms加一节拍；无自动重发。
 */
fw_error_t bsp_kamoer_set_direction(bsp_kamoer_t *self, bsp_kamoer_direction_t direction);
/**
 * @brief 选择速度模式；调用方应先停车。
 * @param[in,out] self 已绑定实例，不得为NULL。
 * @retval FW_OK 模式写入ACK有效。
 * @return 见公共事务错误契约。
 * @note 任务上下文；至多timeout_ms加一节拍；不启动电机。
 */
fw_error_t bsp_kamoer_set_speed_mode(bsp_kamoer_t *self);
/**
 * @brief 设置起始速度，32位高字在先，写0x4007/0x4008。
 * @param[in,out] self 已绑定实例，不得为NULL。
 * @param[in] speed_centi_rpm 0..50000，单位0.01rpm；建议不大于运行速度。
 * @retval FW_OK 写入ACK有效。
 * @retval FW_ERR_KAMOER_INVALID_ARG 速度越界。
 * @return 见公共事务错误契约。
 * @note 任务上下文；至多timeout_ms加一节拍；不写Flash。
 */
fw_error_t bsp_kamoer_set_start_speed(bsp_kamoer_t *self, uint32_t speed_centi_rpm);
/**
 * @brief 设置运行速度，写0x4009/0x400A。
 * @param[in,out] self 已绑定实例，不得为NULL。
 * @param[in] speed_centi_rpm 100..50000，单位0.01rpm。
 * @retval FW_OK 写入ACK有效。
 * @retval FW_ERR_KAMOER_INVALID_ARG 速度越界。
 * @return 见公共事务错误契约。
 * @note 任务上下文；至多timeout_ms加一节拍；不写Flash。
 */
fw_error_t bsp_kamoer_set_speed(bsp_kamoer_t *self, uint32_t speed_centi_rpm);
/**
 * @brief 启动电机；写线圈0x0001=0xFF00，只确认ACK。
 * @param[in,out] self 已绑定且完成参数配置的实例，不得为NULL。
 * @retval FW_OK 启动命令ACK有效，电机可能仍在加速。
 * @return 见公共事务错误契约；未知结果必须对账，禁止自动重复启动。
 * @note 任务上下文；至多timeout_ms加一节拍；长运动由应用分步监控。
 */
fw_error_t bsp_kamoer_start(bsp_kamoer_t *self);
/**
 * @brief 显式发送一次停车命令；允许FAULT时执行，不清除首个失败。
 * @param[in,out] self 已绑定实例，不得为NULL。
 * @retval FW_OK 停车ACK有效，不保证轴已静止或管路已停止出液。
 * @return 见公共事务错误契约；FAULT时失败另记cleanup_code。
 * @note 任务上下文；至多timeout_ms加一节拍；不会隐式重试。
 */
fw_error_t bsp_kamoer_stop(bsp_kamoer_t *self);
/**
 * @brief 一次读取0x3001起8个输入寄存器。
 * @param[in,out] self 已绑定实例，不得为NULL。
 * @param[out] feedback 输出快照，不得为NULL；失败时保持原值。
 * @retval FW_OK 已读取有效帧；不能由位置脉冲推断实际滴数。
 * @retval FW_ERR_KAMOER_INVALID_ARG 参数无效。
 * @return 见公共事务错误契约。
 * @note 任务上下文；至多timeout_ms加一节拍；FAULT轮询无新IO。
 */
fw_error_t bsp_kamoer_read_feedback(bsp_kamoer_t *self, bsp_kamoer_feedback_t *feedback);
/**
 * @brief 查询通信状态及首错快照。
 * @param[in] self 实例，不得为NULL，初始化失败后也可查询。
 * @param[out] state 状态，不得为NULL。
 * @param[out] error 首错，不得为NULL。
 * @retval FW_OK 已复制。
 * @retval FW_ERR_KAMOER_INVALID_ARG 参数为空，输出不变。
 * @note 不阻塞；同实例由调用者串行化。
 */
fw_error_t bsp_kamoer_get_status(const bsp_kamoer_t *self, bsp_kamoer_state_t *state,
                                fw_error_record_t *error);
/**
 * @brief 计算CRC16/MODBUS，初值0xFFFF、多项式0xA001，线上低字节在先。
 * @param[in] data 数据；bytes为0时可为NULL。
 * @param[in] bytes 数据字节数。
 * @return CRC；data为空且bytes非0时返回0。
 * @note 纯计算，不阻塞，无共享状态。
 */
uint16_t bsp_kamoer_crc16(const uint8_t *data, size_t bytes);
/**
 * @details 公共事务错误契约：FW_ERR_KAMOER_INVALID_ARG参数非法；
 * FW_ERR_KAMOER_NOT_READY未绑定；FW_ERR_KAMOER_INVALID_STATE非法状态；
 * FW_ERR_KAMOER_IO总线失败；FW_ERR_KAMOER_TIMEOUT读取超时；
 * FW_ERR_KAMOER_CRC校验失败；FW_ERR_KAMOER_RESPONSE帧/回显错误；
 * FW_ERR_KAMOER_EXCEPTION设备异常（native_status保存异常码）；
 * FW_ERR_KAMOER_EXECUTION_UNKNOWN写命令执行结果未知，cause_code保留原因。
 * 除非法参数/未绑定外，上述错误锁存FAULT；FAULT返回原首错且不发普通请求。
 */
#endif
