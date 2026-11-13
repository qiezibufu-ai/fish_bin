/**
 * @file osal_diagnostics.h
 * @brief 定义 OSAL 内核堆余量、任务数量与最小栈水位诊断接口。
 */
#ifndef AD_PLATFORM_OS_INC_OSAL_DIAGNOSTICS_H
#define AD_PLATFORM_OS_INC_OSAL_DIAGNOSTICS_H

#include "osal_status.h"
#include "osal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief OSAL 运行时堆与任务统计快照。 */
typedef struct
{
    size_t free_heap_bytes;         /**< 当前剩余可用堆字节数，单位bytes。 */
    size_t minimum_free_heap_bytes; /**< 启动以来历史最低可用堆字节数，单位bytes。 */
    uint32_t task_count;            /**< 当前内核已注册的活动任务数量。 */
} osal_runtime_info_t;

/**
 * @brief 读取当前 RTOS 内核堆余量与活动任务数快照。
 * @param[out] out 接收诊断快照的指针，不得为NULL；失败时保持原值。
 * @retval OSAL_OK 快照读取成功。
 * @retval OSAL_ERR_PARAM out 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用，不阻塞。
 */
osal_status_t osal_runtime_info(osal_runtime_info_t *out);

/**
 * @brief 查询指定任务自启动以来的历史最小剩余栈空间（高水位余量）。
 * @param[in] handle 目标任务句柄；传NULL时返回0。
 * @return 最小剩余栈字节数，单位bytes；在 ISR 中或句柄无效时返回0。
 * @note 仅任务上下文调用，不阻塞。
 */
size_t osal_task_min_stack_bytes(osal_task_handle_t handle);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_OS_INC_OSAL_DIAGNOSTICS_H */
