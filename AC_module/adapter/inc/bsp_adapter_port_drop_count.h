/** @file bsp_adapter_port_drop_count.h @brief 连接PCNT采集与滴定输入快照契约。 */
#ifndef AC_MODULE_ADAPTER_INC_BSP_ADAPTER_PORT_DROP_COUNT_H
#define AC_MODULE_ADAPTER_INC_BSP_ADAPTER_PORT_DROP_COUNT_H

#include "bsp_wrapper_dosing_inputs.h"

/**
 * @brief 读取硬件累计滴数和采样时间；气泡通道仍未知。
 * @param[in] context 当前单板实例不需要上下文，传NULL。
 * @param[out] snapshot 快照，不得为NULL；失败时保持原值。
 * @retval FW_OK 已复制有效计数；liquid_valid为false。
 * @retval FW_ERR_PORT_INVALID_ARG 快照指针为空。
 * @retval FW_ERR_PORT_NOT_READY PCNT尚未启动。
 * @return 其他PCNT错误原样传播。
 * @note 任务上下文；只读且不等待脉冲；与PCNT生命周期串行。
 */
fw_error_t bsp_adapter_port_drop_count_read(void *context, bsp_dosing_inputs_snapshot_t *snapshot);

#endif
