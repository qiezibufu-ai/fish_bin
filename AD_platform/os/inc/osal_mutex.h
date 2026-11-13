/**
 * @file osal_mutex.h
 * @brief 定义 OSAL 互斥锁接口（支持优先级继承，仅限任务上下文调用）。
 */
#ifndef AD_PLATFORM_OS_INC_OSAL_MUTEX_H
#define AD_PLATFORM_OS_INC_OSAL_MUTEX_H

#include "osal_status.h"
#include "osal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建一个支持优先级继承的互斥锁。
 * @param[out] mutex_handle 输出互斥锁句柄，不得为NULL；失败时置为NULL。
 * @retval OSAL_OK 创建成功。
 * @retval OSAL_ERR_PARAM mutex_handle 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_NO_MEMORY 内核内存不足。
 * @note 仅任务上下文调用；新创建的互斥锁处于未加锁状态。
 */
osal_status_t osal_mutex_create(osal_mutex_handle_t *mutex_handle);

/**
 * @brief 删除互斥锁并释放内核资源。
 * @param[in] mutex_handle 待删除的互斥锁句柄，不得为NULL。
 * @retval OSAL_OK 删除成功。
 * @retval OSAL_ERR_PARAM mutex_handle 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用；调用者必须确保无任务正在等待或持有该锁。
 */
osal_status_t osal_mutex_delete(osal_mutex_handle_t mutex_handle);

/**
 * @brief 在任务上下文释放当前任务持有的互斥锁。
 * @param[in] mutex_handle 待释放的互斥锁句柄，不得为NULL。
 * @retval OSAL_OK 释放成功。
 * @retval OSAL_ERR_PARAM mutex_handle 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_STATE 当前任务并非该互斥锁的持有者。
 * @note 仅任务上下文调用；禁止在 ISR 中操作互斥锁。
 */
osal_status_t osal_mutex_give(osal_mutex_handle_t mutex_handle);

/**
 * @brief 在任务上下文申请获取互斥锁，超时单位为毫秒。
 * @param[in] mutex_handle 待申请的互斥锁句柄，不得为NULL。
 * @param[in] timeout_ms 最大等待时间，单位ms；0表示非阻塞尝试，OSAL_MAX_DELAY表示永久等待。
 * @retval OSAL_OK 成功获取互斥锁。
 * @retval OSAL_ERR_PARAM mutex_handle 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_TIMEOUT 在 timeout_ms 预算内未能获取互斥锁。
 * @note 仅任务上下文调用；不持状态锁等待慢速硬件或 sleep。
 */
osal_status_t osal_mutex_take(osal_mutex_handle_t mutex_handle,
                              osal_tick_type_t timeout_ms);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_OS_INC_OSAL_MUTEX_H */
