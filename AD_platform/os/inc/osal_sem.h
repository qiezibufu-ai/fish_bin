/**
 * @file osal_sem.h
 * @brief 定义 OSAL 二值信号量与计数信号量接口（自动适配任务与 ISR 上下文）。
 */
#ifndef AD_PLATFORM_OS_INC_OSAL_SEM_H
#define AD_PLATFORM_OS_INC_OSAL_SEM_H

#include "osal_status.h"
#include "osal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建一个二值信号量（初始状态为空/不可获取，须先 give 后才能 take）。
 * @param[out] sema_handle 输出信号量句柄，不得为NULL；失败时置为NULL。
 * @retval OSAL_OK 创建成功。
 * @retval OSAL_ERR_PARAM sema_handle 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_NO_MEMORY 内核内存不足。
 * @note 仅任务上下文调用。
 */
osal_status_t osal_sema_binary_create(osal_sema_handle_t *sema_handle);

/**
 * @brief 创建一个计数信号量。
 * @param[out] sema_handle 输出信号量句柄，不得为NULL；失败时置为NULL。
 * @param[in] max_count 最大计数值，必须大于0。
 * @param[in] init_count 初始计数值，范围 0..max_count。
 * @retval OSAL_OK 创建成功。
 * @retval OSAL_ERR_PARAM 参数为空、max_count为0或 init_count > max_count。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_NO_MEMORY 内核内存不足。
 * @note 仅任务上下文调用。
 */
osal_status_t osal_sema_counting_create(osal_sema_handle_t *sema_handle,
                                        uint32_t max_count,
                                        uint32_t init_count);

/**
 * @brief 兼容旧工程命名的计数信号量创建接口。
 * @param[out] sema_handle 输出信号量句柄，不得为NULL。
 * @param[in] max_count 最大计数值，必须大于0。
 * @param[in] init_count 初始计数值，范围 0..max_count。
 * @return 与 osal_sema_counting_create() 相同的状态码。
 * @note 薄兼容内联包装，新代码优先调用 osal_sema_counting_create()。
 */
static inline osal_status_t osal_sema_countings_create(osal_sema_handle_t *sema_handle,
                                                       uint32_t max_count,
                                                       uint32_t init_count)
{
    return osal_sema_counting_create(sema_handle, max_count, init_count);
}

/**
 * @brief 删除信号量并释放内核资源。
 * @param[in] sema_handle 待删除的信号量句柄，不得为NULL。
 * @retval OSAL_OK 删除成功。
 * @retval OSAL_ERR_PARAM sema_handle 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用；删除前须确保无任务阻塞等待该信号量。
 */
osal_status_t osal_sema_delete(osal_sema_handle_t sema_handle);

/**
 * @brief 释放（V操作）信号量，自动适配任务与 ISR 上下文。
 * @param[in] sema_handle 目标信号量句柄，不得为NULL。
 * @retval OSAL_OK 释放成功。
 * @retval OSAL_ERR_PARAM sema_handle 为NULL。
 * @retval OSAL_ERR_STATE 信号量计数已达上限（二值已满或计数达 max_count）。
 * @note 可在任务或 ISR 上下文调用；ISR 中必要时自动触发高优先级任务切换。
 */
osal_status_t osal_sema_give(osal_sema_handle_t sema_handle);

/**
 * @brief 获取（P操作）信号量，自动适配任务与 ISR 上下文。
 * @param[in] sema_handle 目标信号量句柄，不得为NULL。
 * @param[in] timeout_ms 任务上下文最大等待时间，单位ms（OSAL_MAX_DELAY表示永久等待）；ISR 中忽略该值并执行非阻塞获取。
 * @retval OSAL_OK 获取成功。
 * @retval OSAL_ERR_PARAM sema_handle 为NULL。
 * @retval OSAL_ERR_TIMEOUT 超时或 ISR 中当前无可用信号量。
 * @note 可在任务或 ISR 上下文调用；ISR 中严禁阻塞。
 */
osal_status_t osal_sema_take(osal_sema_handle_t sema_handle,
                             osal_tick_type_t timeout_ms);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_OS_INC_OSAL_SEM_H */
