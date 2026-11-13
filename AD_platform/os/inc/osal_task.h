/**
 * @file osal_task.h
 * @brief 定义 OSAL 任务管理、周期调度、系统节拍与临界区接口。
 */
#ifndef AD_PLATFORM_OS_INC_OSAL_TASK_H
#define AD_PLATFORM_OS_INC_OSAL_TASK_H

#include "osal_status.h"
#include "osal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建并启动一个 RTOS 任务，栈大小统一以字节（bytes）为单位。
 * @param[in] task_name 任务名称字符串，不得为NULL，含'\0'长度不超过 OSAL_NAME_MAX_LEN。
 * @param[in] entry 任务入口函数，不得为NULL。
 * @param[in] stack_size 任务栈大小，单位bytes，必须大于0。
 * @param[in] priority 任务数值优先级，必须小于内核最大优先级。
 * @param[out] task_handle 可选输出任务句柄，可为NULL；失败时置为NULL。
 * @param[in] argument 传递给 entry 的上下文指针，可为NULL。
 * @retval OSAL_OK 任务创建成功。
 * @retval OSAL_ERR_PARAM 参数为空、名称超长、stack_size为0或优先级越界。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_NO_MEMORY 内核堆空间不足，未能创建任务。
 * @note 仅任务上下文调用；按架构规范应在 AA_app 组合根统一注册创建。
 */
osal_status_t osal_task_create(const char *task_name,
                               osal_task_entry entry,
                               size_t stack_size,
                               osal_priority_t priority,
                               osal_task_handle_t *task_handle,
                               void *argument);

/**
 * @brief 删除指定任务或当前自身任务。
 * @param[in] task_handle 目标任务句柄；传NULL表示删除当前正在执行的任务。
 * @retval OSAL_OK 删除成功（删除自身时不会返回）。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用；调用者须在删除前释放任务持有的互斥锁与业务资源。
 */
osal_status_t osal_task_delete(osal_task_handle_t task_handle);

/**
 * @brief 启动 RTOS 调度器。
 * @retval OSAL_OK 在 ESP-IDF 等已由引导代码启动调度器的平台上直接返回成功。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_NO_MEMORY 裸启动环境下调度器启动失败返回。
 * @note 仅任务或启动上下文调用。
 */
osal_status_t osal_task_start(void);

/**
 * @brief 挂起指定任务或当前任务。
 * @param[in] task_handle 目标任务句柄；传NULL表示挂起当前任务。
 * @retval OSAL_OK 挂起成功。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用。
 */
osal_status_t osal_task_suspend(osal_task_handle_t task_handle);

/**
 * @brief 恢复已挂起的任务，自动适配任务与 ISR 上下文。
 * @param[in] task_handle 待恢复的任务句柄，不得为NULL。
 * @retval OSAL_OK 恢复请求已提交。
 * @retval OSAL_ERR_PARAM task_handle 为NULL。
 * @note 可在任务或 ISR 上下文调用；ISR 中必要时自动触发上下文切换。
 */
osal_status_t osal_task_resume(osal_task_handle_t task_handle);

/**
 * @brief 挂起当前核任务调度器，不关闭硬件中断。
 * @retval OSAL_OK 调度器已挂起。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用；挂起期间禁止调用任何可能阻塞的 OSAL API。
 */
osal_status_t osal_task_suspend_all(void);

/**
 * @brief 按原始 OS tick 数延时当前任务。
 * @param[in] ticks 延时的原始系统节拍数。
 * @retval OSAL_OK 延时完成。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用；业务层优先使用带明确单位的 osal_task_delay_ms()。
 */
osal_status_t osal_task_delay(uint32_t ticks);

/**
 * @brief 按毫秒延时当前任务，非零毫秒向上取整为至少1个 tick。
 * @param[in] delay_ms 延时时间，单位ms；不得为 OSAL_MAX_DELAY。
 * @retval OSAL_OK 延时完成。
 * @retval OSAL_ERR_PARAM delay_ms 为 OSAL_MAX_DELAY。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用；最多阻塞约 delay_ms 加1个系统 tick。
 */
osal_status_t osal_task_delay_ms(uint32_t delay_ms);

/**
 * @brief 基于上一次唤醒节拍执行固定周期延时等待。
 * @param[in,out] last_wake 上一次唤醒节拍指针，不得为NULL；返回前自动更新为新唤醒点。
 * @param[in] period_ms 周期时长，单位ms，范围 1..(OSAL_MAX_DELAY-1)。
 * @retval OSAL_OK 周期等待完成。
 * @retval OSAL_ERR_PARAM last_wake 为NULL或 period_ms 无效。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用；首次调用前应以 osal_task_get_tick_count() 初始化 *last_wake。
 */
osal_status_t osal_task_wait_until(osal_tick_type_t *last_wake,
                                   uint32_t period_ms);

/**
 * @brief 进入 OSAL 跨核自旋锁临界区。
 * @retval OSAL_OK 已进入临界区。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用；必须与 osal_exit_critical() 成对调用，临界区内严禁阻塞。
 */
osal_status_t osal_enter_critical(void);

/**
 * @brief 退出 OSAL 跨核自旋锁临界区。
 * @retval OSAL_OK 已退出临界区。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用，必须与 osal_enter_critical() 严格配对。
 */
osal_status_t osal_exit_critical(void);

/**
 * @brief 当前任务主动让出 CPU 时间片。
 * @retval OSAL_OK 已触发让出。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用。
 */
osal_status_t osal_port_yield(void);

/**
 * @brief 开启当前 CPU 核中断。
 * @retval OSAL_OK 已开启中断。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用；优先使用 osal_enter_critical()/osal_exit_critical()。
 */
osal_status_t osal_task_enable_interrupts(void);

/**
 * @brief 关闭当前 CPU 核可屏蔽中断。
 * @retval OSAL_OK 已关闭中断。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用。
 */
osal_status_t osal_task_disable_interrupts(void);

/**
 * @brief 获取当前系统单调 tick 计数，自动适配任务与 ISR 上下文。
 * @return 当前系统节拍计数（允许32位无符号回绕）。
 * @note 不阻塞，可在任务或 ISR 上下文安全调用。
 */
osal_tick_type_t osal_task_get_tick_count(void);

/**
 * @brief 将两个系统 tick 差值换算为毫秒。
 * @param[in] now 当前 tick 值。
 * @param[in] old 历史起始 tick 值。
 * @return 经过的毫秒数，单位ms；若 now 早于 old（按有符号差判断）则返回 0。
 * @note 纯计算函数，时间间隔须小于 2^31 个 tick。
 */
uint32_t osal_task_elapsed_ms(osal_tick_type_t now, osal_tick_type_t old);

/**
 * @brief 获取系统单调运行毫秒时间戳。
 * @return 当前单调毫秒时间，单位ms（允许 uint32_t 回绕）。
 * @note 不阻塞，可在任务或 ISR 上下文调用，供 FSM 与错误快照记录 timestamp_ms。
 */
uint32_t osal_task_now_ms(void);

/**
 * @brief 获取当前正在运行的任务句柄。
 * @return 当前任务句柄；在 ISR 中或调度前可能返回关联当前核的任务句柄。
 * @note 不阻塞。
 */
osal_task_handle_t osal_task_get_current_handle(void);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_OS_INC_OSAL_TASK_H */
