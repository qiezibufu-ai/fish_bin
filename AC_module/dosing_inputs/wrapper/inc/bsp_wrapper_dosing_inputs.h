/** @file bsp_wrapper_dosing_inputs.h @brief 滴定输入预留契约；无实现时必须报告未就绪。 */
#ifndef AC_MODULE_DOSING_INPUTS_WRAPPER_INC_BSP_WRAPPER_DOSING_INPUTS_H
#define AC_MODULE_DOSING_INPUTS_WRAPPER_INC_BSP_WRAPPER_DOSING_INPUTS_H
#include "fw_error.h"
/** @brief 气泡/液体状态；禁止把缺失/未标定传感器当作有药。 */
typedef enum
{
    BSP_DOSING_LIQUID_UNKNOWN = 0, /**< 未接入、未标定或过期。 */
    BSP_DOSING_LIQUID_PRESENT,     /**< 已标定信号指示管路有液。 */
    BSP_DOSING_LIQUID_ABSENT       /**< 已标定信号指示空气/无液。 */
} bsp_dosing_liquid_state_t;
/** @brief 带有效性和采样时间的滴定输入快照，不是电机脉冲。 */
typedef struct
{
    bsp_dosing_liquid_state_t liquid; /**< 气泡传感器判定，默认UNKNOWN。 */
    bool liquid_valid;                /**< 气泡通道已标定且样本有效。 */
    uint32_t drop_count;               /**< QLT1616累计液滴计数，允许32位回绕。 */
    bool count_valid;                 /**< 计数通道已接入且计数窗口有效。 */
    uint32_t timestamp_ms;             /**< 采样单调毫秒时间。 */
} bsp_dosing_inputs_snapshot_t;
/** @brief 后续滴定服务注入的输入端口；本期无提供者，read_snapshot为NULL。 */
typedef struct
{
    /**
     * @brief 读取一致快照，不重置硬件计数。
     * @param[in] context 提供者上下文，寿命覆盖服务。
     * @param[out] snapshot 输出快照，不得为NULL；失败时保持原值。
     * @retval FW_OK 已复制快照，各字段仍须检查valid及年龄。
     * @retval FW_ERR_PORT_NOT_READY 未接入或尚未标定。
     * @return 后续提供者的具体错误，不得用空回调返回假成功。
     * @note 任务上下文，非阻塞；提供者负责ISR计数与任务读取的一致性。
     */
    fw_error_t (*read_snapshot)(void *context, bsp_dosing_inputs_snapshot_t *snapshot);
    void *context; /**< 提供者所有的上下文；未绑定为NULL。 */
} bsp_dosing_inputs_ops_t;
#endif
