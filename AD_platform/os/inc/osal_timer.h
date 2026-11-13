/**
 * @file osal_timer.h
 * @brief 定义 OSAL 软件定时器接口（周期与等待时间均以毫秒为单位）。
 */
#ifndef AD_PLATFORM_OS_INC_OSAL_TIMER_H
#define AD_PLATFORM_OS_INC_OSAL_TIMER_H

#include "osal_status.h"
#include "osal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建一个软件定时器（创建后处于未启动休眠态，需显式调用 osal_timer_start 启动）。
 * @param[out] timer_handle 输出定时器句柄，不得为NULL；失败时置为NULL。
 * @param[in] timer_name 定时器名称，不得为NULL，含'\0'长度不超过 OSAL_NAME_MAX_LEN。
 * @param[in] timer_period_ms 定时器周期，单位ms，必须大于0且不为 OSAL_MAX_DELAY。
 * @param[in] auto_reload 非0表示周期自动重载定时器，0表示单次触发（One-shot）定时器。
 * @param[in] timer_cb 定时器到期回调函数，不得为NULL。
 * @param[in] arg 传递给 timer_cb 的用户上下文指针，可为NULL。
 * @retval OSAL_OK 创建成功。
 * @retval OSAL_ERR_PARAM 参数为空、名称超长或周期无效。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_NO_MEMORY 内核或堆内存不足。
 * @note 仅任务上下文调用；timer_cb 在定时器服务任务中执行，不得阻塞。
 */
osal_status_t osal_timer_create(osal_timer_handle_t *timer_handle,
                                const char *timer_name,
                                osal_tick_type_t timer_period_ms,
                                uint8_t auto_reload,
                                osal_timer_cb_function_t timer_cb,
                                void *arg);

/**
 * @brief 启动或重新启动软件定时器，自动适配任务与 ISR 上下文。
 * @param[in] timer_handle 目标定时器句柄，不得为NULL。
 * @param[in] timeout_ms 任务上下文向定时器命令队列投递的最大等待时间，单位ms；ISR 中忽略该值。
 * @retval OSAL_OK 启动命令已成功投递。
 * @retval OSAL_ERR_PARAM timer_handle 为NULL。
 * @retval OSAL_ERR_TIMEOUT 定时器命令队列满且等待超时。
 * @note 可在任务或 ISR 上下文调用。
 */
osal_status_t osal_timer_start(osal_timer_handle_t timer_handle,
                               osal_tick_type_t timeout_ms);

/**
 * @brief 停止软件定时器，自动适配任务与 ISR 上下文。
 * @param[in] timer_handle 目标定时器句柄，不得为NULL。
 * @param[in] timeout_ms 任务上下文向定时器命令队列投递的最大等待时间，单位ms；ISR 中忽略该值。
 * @retval OSAL_OK 停止命令已成功投递。
 * @retval OSAL_ERR_PARAM timer_handle 为NULL。
 * @retval OSAL_ERR_TIMEOUT 定时器命令队列满且等待超时。
 * @note 可在任务或 ISR 上下文调用；投递成功前已在执行的回调不会被打断。
 */
osal_status_t osal_timer_stop(osal_timer_handle_t timer_handle,
                              osal_tick_type_t timeout_ms);

/**
 * @brief 修改软件定时器的触发周期（若原本未运行，修改周期同时会启动该定时器）。
 * @param[in] timer_handle 目标定时器句柄，不得为NULL。
 * @param[in] new_period_ms 新的定时周期，单位ms，必须大于0且不为 OSAL_MAX_DELAY。
 * @param[in] timeout_ms 任务上下文命令队列等待时间，单位ms；ISR 中忽略该值。
 * @retval OSAL_OK 改周期命令已成功投递。
 * @retval OSAL_ERR_PARAM timer_handle 为NULL或 new_period_ms 无效。
 * @retval OSAL_ERR_TIMEOUT 定时器命令队列满且等待超时。
 * @note 可在任务或 ISR 上下文调用。
 */
osal_status_t osal_timer_period_change(osal_timer_handle_t timer_handle,
                                       osal_tick_type_t new_period_ms,
                                       osal_tick_type_t timeout_ms);

/**
 * @brief 重置软件定时器计时起点，自动适配任务与 ISR 上下文。
 * @param[in] timer_handle 目标定时器句柄，不得为NULL。
 * @param[in] timeout_ms 任务上下文命令队列等待时间，单位ms；ISR 中忽略该值。
 * @retval OSAL_OK 重置命令已成功投递。
 * @retval OSAL_ERR_PARAM timer_handle 为NULL。
 * @retval OSAL_ERR_TIMEOUT 定时器命令队列满且等待超时。
 * @note 可在任务或 ISR 上下文调用。
 */
osal_status_t osal_timer_reset(osal_timer_handle_t timer_handle,
                               osal_tick_type_t timeout_ms);

/**
 * @brief 删除软件定时器并释放关联资源。
 * @param[in] timer_handle 待删除的定时器句柄，不得为NULL。
 * @param[in] timeout_ms 任务上下文等待删除命令入队的最大时间，单位ms。
 * @retval OSAL_OK 删除成功。
 * @retval OSAL_ERR_PARAM timer_handle 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_TIMEOUT 定时器命令队列满且等待超时，定时器未删除。
 * @note 仅任务上下文调用。
 */
osal_status_t osal_timer_delete(osal_timer_handle_t timer_handle,
                                osal_tick_type_t timeout_ms);

/**
 * @brief 获取软件定时器当前配置的周期毫秒值。
 * @param[in] timer_handle 目标定时器句柄；传NULL时返回0。
 * @return 当前定时器周期，单位ms。
 * @note 不阻塞，可在任务上下文调用。
 */
osal_tick_type_t osal_timer_period_get(osal_timer_handle_t timer_handle);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_OS_INC_OSAL_TIMER_H */
