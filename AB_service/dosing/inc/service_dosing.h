/** @file service_dosing.h @brief 按液滴计数控制单路泵的服务；由上层单任务驱动。 */
#ifndef AB_SERVICE_DOSING_INC_SERVICE_DOSING_H
#define AB_SERVICE_DOSING_INC_SERVICE_DOSING_H
#include "bsp_wrapper_dosing_inputs.h"

/** @brief 泵端口操作；每次调用只提交一条设备命令。 */
typedef enum
{
    SERVICE_DOSING_PUMP_STOP,        /**< 请求停车。 */
    SERVICE_DOSING_PUMP_DIRECTION,   /**< 参数0顺时针、1逆时针。 */
    SERVICE_DOSING_PUMP_SPEED_MODE,  /**< 选择速度模式。 */
    SERVICE_DOSING_PUMP_START_SPEED, /**< 参数为起始速度，0.01rpm。 */
    SERVICE_DOSING_PUMP_SPEED,       /**< 参数为运行速度，0.01rpm。 */
    SERVICE_DOSING_PUMP_START        /**< 触发一次运行。 */
} service_dosing_pump_op_t;

/** @brief 服务阶段及终态。 */
typedef enum
{
    SERVICE_DOSING_IDLE,        /**< 已绑定，等待请求。 */
    SERVICE_DOSING_PRESTOP,     /**< 先确认停车命令ACK。 */
    SERVICE_DOSING_DIRECTION,   /**< 配置方向。 */
    SERVICE_DOSING_MODE,        /**< 配置速度模式。 */
    SERVICE_DOSING_START_SPEED, /**< 配置起始速度。 */
    SERVICE_DOSING_SPEED,       /**< 配置运行速度。 */
    SERVICE_DOSING_START,       /**< 读取计数基线并发出START。 */
    SERVICE_DOSING_RUNNING,     /**< 监测累计滴数和超时。 */
    SERVICE_DOSING_TAIL,        /**< STOP已ACK，等待尾滴安静窗口。 */
    SERVICE_DOSING_DONE,        /**< 达到阈值，STOP已ACK且尾滴窗口已安静。 */
    SERVICE_DOSING_CANCELLED,   /**< 请求取消；若曾运行，已确认STOP应答并观察尾滴。 */
    SERVICE_DOSING_FAULT        /**< 首错已锁存；不得自动重启。 */
} service_dosing_state_t;

/** @brief 已确认的最新计数与泵事务由调用方适配。 */
typedef struct
{
    bsp_dosing_inputs_ops_t inputs; /**< 硬件累计滴数快照入口；气泡字段不参与决策。 */
    /**
     * @brief 提交一条泵命令并等待应答。
     * @param[in] context 泵实例，寿命覆盖服务。
     * @param[in] op 命令类型。
     * @param[in] value 命令参数，未使用时为0。
     * @param[out] error 可用的底层错误详情，无记录则保持零值。
     * @retval FW_OK 设备应答有效，不证明物理动作完成。
     * @return 设备错误原样返回；START结果未知时不得重发。
     * @note 任务上下文；单次事务由适配器限制在250ms配置预算及调度误差内。
     */
    fw_error_t (*pump)(void *context, service_dosing_pump_op_t op, uint32_t value,
                       fw_error_record_t *error);
    void *pump_context; /**< 泵端口借用上下文。 */
    /**
     * @brief 获取单调毫秒时间，允许32位回绕。
     * @param[in] context 时间端口上下文，可为空。
     * @return 当前单调毫秒时间。
     * @note 不等待，任务上下文。
     */
    uint32_t (*now_ms)(void *context);
    void *clock_context; /**< 时钟端口借用上下文。 */
    /**
     * @brief 查询输入采集故障详情。
     * @param[in] context 与inputs共用的上下文。
     * @param[out] error 底层首错记录，无记录时保持零值。
     * @note 可选；只读且不等待。
     */
    void (*input_error)(void *context, fw_error_record_t *error);
} service_dosing_ops_t;

/** @brief 单次滴定请求，所有时限为单调毫秒。 */
typedef struct
{
    uint32_t target_drops;    /**< 停泵阈值，1..10000滴。 */
    uint32_t speed_centi_rpm; /**< 速度，100..50000（0.01rpm）；起始速度等于运行速度。 */
    bool reverse;             /**< true为逆时针，false为顺时针。 */
    uint32_t no_drop_ms;      /**< 首滴或相邻滴数无增长超时，1..max_run_ms。 */
    uint32_t max_run_ms;      /**< 从发START前起算，1..60000ms。 */
    uint32_t tail_quiet_ms;   /**< STOP应答后最少连续无滴时间，1..tail_max_ms。 */
    uint32_t tail_max_ms;     /**< 尾滴观察总上限，1..10000ms。 */
} service_dosing_request_t;

/** @brief 只读结果；FAULT中的滴数可能仅为最后一次成功采样。 */
typedef struct
{
    service_dosing_state_t state; /**< 当前阶段。 */
    uint32_t request_id;          /**< 请求标识，0表示未提交。 */
    uint32_t target_drops;        /**< 请求停泵阈值。 */
    uint32_t drops;               /**< 自START前计数基线起的最后采样值。 */
    uint32_t stop_drops;          /**< 发送本次STOP前最近采样的滴数。 */
    bool count_valid;             /**< 已有有效基线，且最近一次计数读取有效。 */
    bool stop_acked;              /**< 最后一条必要STOP收到应答，非物理停机证明。 */
    fw_error_record_t error;      /**< 首错、底层原因及独立清理错误。 */
} service_dosing_result_t;

/** @brief 调用方静态分配的实例；字段仅由服务实现写入。 */
typedef struct
{
    service_dosing_ops_t ops;         /**< 端口绑定。 */
    service_dosing_request_t request; /**< 当前请求副本。 */
    service_dosing_result_t result;   /**< 可通过查询API复制的快照。 */
    uint32_t baseline;                /**< START前的硬件累计基线。 */
    uint32_t started_ms;              /**< START发送前时间。 */
    uint32_t last_drop_ms;            /**< 最近观察到计数增长的时间。 */
    uint32_t stopped_ms;              /**< STOP应答时间。 */
    bool bound;                       /**< 已完成绑定。 */
    bool motion_possible;             /**< START可能已执行且尚未确认STOP。 */
    bool cancelled;                   /**< 当前尾滴观察对应取消请求。 */
} service_dosing_t;

/**
 * @brief 绑定端口并置空闲，不访问泵。
 * @param[out] self 新实例，或已在现场确认停止并恢复端口的实例，不得为空。
 * @param[in] ops 端口表，复制保存；回调及上下文须覆盖服务寿命。
 * @retval FW_OK 已绑定。
 * @retval FW_ERR_DOSING_ARG 缺少实例或必需端口。
 * @note 不阻塞；所有接口仅由一个所有者任务串行调用；不得用初始化替代停车。
 */
fw_error_t service_dosing_init(service_dosing_t *self, const service_dosing_ops_t *ops);
/**
 * @brief 接受一次请求，复制参数；由后续step推进，不立即发命令。
 * @param[in,out] self 已绑定实例。
 * @param[in] request 非空且参数符合字段范围。
 * @retval FW_OK 已受理。
 * @retval FW_ERR_DOSING_ARG 参数无效。
 * @retval FW_ERR_DOSING_BUSY 正在处理请求。
 * @return 未初始化或故障返回FW_ERR_DOSING_NOT_READY或锁存首错。
 * @note 任务上下文，正常路径不等待；非法内部状态可能尝试一次清理STOP。
 * DONE和CANCELLED允许下一请求。
 */
fw_error_t service_dosing_start(service_dosing_t *self, const service_dosing_request_t *request);
/**
 * @brief 推进一阶段或检查滴数；FAULT和正常终态轮询无新输出。
 * @param[in,out] self 已绑定实例。
 * @retval FW_OK 已推进或正在等待。
 * @retval FW_ERR_DOSING_ARG 实例为空。
 * @retval FW_ERR_DOSING_NOT_READY 尚未绑定。
 * @return 状态推进失败时返回锁存错误：FW_ERR_DOSING_SETUP、FW_ERR_DOSING_START、
 * FW_ERR_DOSING_STOP、FW_ERR_DOSING_COUNT、FW_ERR_DOSING_NO_DROP、
 * FW_ERR_DOSING_RUN_TIMEOUT、FW_ERR_DOSING_TAIL_TIMEOUT或FW_ERR_DOSING_STATE。
 * @note 任务上下文；通常最多一条泵事务；失败可额外发一次清理STOP。
 */
fw_error_t service_dosing_step(service_dosing_t *self);
/**
 * @brief 取消当前请求；若泵可能运动，立即尝试STOP并进入尾滴观察。
 * @param[in,out] self 已绑定实例。
 * @retval FW_OK 取消已受理或实例无活动请求；最终状态通过结果查询。
 * @return 参数、未初始化或锁存错误。
 * @note 任务上下文；最多一条泵事务；FAULT不会重试。
 */
fw_error_t service_dosing_cancel(service_dosing_t *self);
/**
 * @brief 复制最新结果，不触发IO。
 * @param[in] self 已绑定实例。
 * @param[out] result 非空；失败时保持原值。
 * @retval FW_OK 已复制，包括FAULT结果。
 * @retval FW_ERR_DOSING_ARG 参数为空。
 * @retval FW_ERR_DOSING_NOT_READY 尚未绑定。
 * @note 不阻塞；由所有者串行调用。
 */
fw_error_t service_dosing_get_result(const service_dosing_t *self, service_dosing_result_t *result);
#endif
