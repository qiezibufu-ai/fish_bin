/** @file app_motor_test.h @brief 单路电机台架测试FSM；不表示滴定闭环或实际药量。 */
#ifndef AA_APP_APP_MOTOR_TEST_H
#define AA_APP_APP_MOTOR_TEST_H
#include "bsp_kamoer_driver.h"
/** @brief 台架测试阶段；只由bootstrap任务维护。 */
typedef enum
{
    APP_MOTOR_IDLE = 0,       /**< 已绑定，不发总线命令。 */
    APP_MOTOR_PROBE,          /**< 读取设备反馈及版本。 */
    APP_MOTOR_INITIAL_STOP,   /**< 先发送停车命令。 */
    APP_MOTOR_DIRECTION,      /**< 设置转向。 */
    APP_MOTOR_MODE,           /**< 设置速度模式。 */
    APP_MOTOR_START_SPEED,    /**< 设置起始速度。 */
    APP_MOTOR_SPEED,          /**< 设置运行速度。 */
    APP_MOTOR_START,          /**< 发送一次启动命令。 */
    APP_MOTOR_RUNNING,        /**< 按期限分步监控。 */
    APP_MOTOR_STOP_ACKED,     /**< STOP已ACK；不表示滴定完成或物理轴停止。 */
    APP_MOTOR_CANCELLED,      /**< 显式取消且STOP已ACK。 */
    APP_MOTOR_FAULT           /**< 首错锁存，已尝试一次清理STOP。 */
} app_motor_test_state_t;
/** @brief 静态测试上下文；全部字段只允许本模块API写，其他任务不能直接读取。 */
typedef struct
{
    bsp_kamoer_t motor;                   /**< 驱动实例与底层首错。 */
    app_motor_test_state_t state;         /**< 测试阶段。 */
    fw_error_record_t error;              /**< 应用首错，含STOP清理结果。 */
    bsp_kamoer_feedback_t feedback;        /**< 最后一次成功的电机反馈。 */
    bool feedback_valid;                 /**< 是否成功读取过反馈。 */
    bool stop_acked;                     /**< 最近清理/最终STOP是否收到ACK。 */
    uint32_t started_ms;                  /**< 启动ACK后的单调时间戳。 */
    uint32_t duration_ms;                 /**< 本次运行预算，1..60000ms。 */
    uint32_t speed_centi_rpm;             /**< 本次测试速度，0.01rpm。 */
    bsp_kamoer_direction_t direction;     /**< 本次转向。 */
} app_motor_test_t;
/** @brief 可复制的测试结果；终态查询没有设备副作用。 */
typedef struct
{
    app_motor_test_state_t state;  /**< 当前测试阶段。 */
    fw_error_record_t error;       /**< 首错和STOP清理错误。 */
    bsp_kamoer_feedback_t feedback; /**< 最近电机反馈，须检查有效标志及时间。 */
    bool feedback_valid;          /**< 最近快照是否来自完整有效响应。 */
    bool stop_acked;              /**< 最近STOP已ACK，不代表物理停止。 */
} app_motor_test_result_t;
/**
 * @brief 复制测试结果，不触发串口操作。
 * @param[in] self 已初始化上下文，不得为NULL。
 * @param[out] result 输出结果，不得为NULL，失败保持原值。
 * @retval FW_OK 复制成功，FAULT原因通过error读取。
 * @retval FW_ERR_KAMOER_INVALID_ARG 参数为空。
 * @note 不阻塞；仅由同一个所有者任务调用，跨任务读取须通过外部同步快照。
 */
fw_error_t app_motor_test_get_result(const app_motor_test_t *self,
                                    app_motor_test_result_t *result);
/**
 * @brief 初始化测试上下文并绑定UART1，不访问电机。
 * @param[out] self 清零且无活动运动的上下文，不得为NULL。
 * @retval FW_OK 绑定成功。
 * @retval FW_ERR_KAMOER_INVALID_ARG 参数无效。
 * @note 组合根启动时调用；不阻塞。此后所有API由bootstrap一个任务串行调用。
 */
fw_error_t app_motor_test_init(app_motor_test_t *self);
/**
 * @brief 提交一次速度模式测试；后续step才发送命令。
 * @param[in,out] self 已初始化上下文，不得为NULL。
 * @param[in] direction 顺/逆时针。
 * @param[in] speed_centi_rpm 测试速度100..50000，单位0.01rpm。
 * @param[in] duration_ms 启动ACK后持续1..60000ms。
 * @retval FW_OK 请求接受，尚未启动。
 * @retval FW_ERR_KAMOER_BUSY 已有活动测试。
 * @retval FW_ERR_KAMOER_INVALID_ARG 参数非法。
 * @retval FW_ERR_KAMOER_NOT_READY 尚未绑定。
 * @return FAULT原样返回首错；未知状态返回FW_ERR_APP_MOTOR_INVALID_STATE并锁存。
 * @note 不阻塞，不清除FAULT；只能在确认停机和重建串口后重新init恢复。
 */
fw_error_t app_motor_test_start(app_motor_test_t *self, bsp_kamoer_direction_t direction,
                               uint32_t speed_centi_rpm, uint32_t duration_ms);
/**
 * @brief 推进一步，最多一次普通事务；失败时额外发送一次STOP，不自动重试运动。
 * @param[in,out] self 已初始化上下文，不得为NULL。
 * @retval FW_OK 等待/推进/终态无新副作用，具体阶段见self->state。
 * @retval FW_ERR_KAMOER_INVALID_ARG self为空。
 * @retval FW_ERR_APP_MOTOR_TEST_FAILED 设备事务失败，error.cause_code为具体驱动原因。
 * @retval FW_ERR_APP_MOTOR_INVALID_STATE 非法应用状态。
 * @return FAULT返回已锁存应用首错，记录带阶段、底层位置和独立cleanup_code。
 * @note bootstrap每100ms调用；普通至多250ms，失败清理总计至多500ms加OS节拍。
 * RUNNING按单调时间比较，回绕安全；到期后发STOP，物理停止时间须实测。
 */
fw_error_t app_motor_test_step(app_motor_test_t *self);
/**
 * @brief 显式取消活动测试，立即发送一次STOP；FAULT下允许再次尝试STOP。
 * @param[in,out] self 已初始化上下文，不得为NULL。
 * @retval FW_OK STOP已ACK，普通活动测试转CANCELLED；FAULT保持首错。
 * @retval FW_ERR_KAMOER_INVALID_ARG self为空。
 * @return FAULT时返回本次STOP的具体错误；其他状态STOP失败返回
 * FW_ERR_APP_MOTOR_TEST_FAILED，error.cause_code保留原因；失败不声称已停车。
 * @note 单一所有者任务上下文；至多250ms加一节拍；无自动重发，无迟到完成回调。
 */
fw_error_t app_motor_test_cancel(app_motor_test_t *self);
#endif
