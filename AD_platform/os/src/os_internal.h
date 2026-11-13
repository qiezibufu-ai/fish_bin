/**
 * @file os_internal.h
 * @brief 定义 OSAL 稳定参数校验层与底层 RTOS 实现层之间的私有契约。
 */
#ifndef AD_PLATFORM_OS_SRC_OS_INTERNAL_H
#define AD_PLATFORM_OS_SRC_OS_INTERNAL_H

#include "osal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------ 互斥锁 ------------------------------ */
osal_status_t os_mutex_create(osal_mutex_handle_t *mutex_handle);
osal_status_t os_mutex_delete(osal_mutex_handle_t mutex_handle);
osal_status_t os_mutex_give(osal_mutex_handle_t mutex_handle);
osal_status_t os_mutex_take(osal_mutex_handle_t mutex_handle,
                            osal_tick_type_t timeout_ms);

/* ------------------------------ 信号量 ------------------------------ */
osal_status_t os_sema_binary_create(osal_sema_handle_t *sema_handle);
osal_status_t os_sema_counting_create(osal_sema_handle_t *sema_handle,
                                      uint32_t max_count,
                                      uint32_t init_count);
osal_status_t os_sema_delete(osal_sema_handle_t sema_handle);
osal_status_t os_sema_give(osal_sema_handle_t sema_handle);
osal_status_t os_sema_take(osal_sema_handle_t sema_handle,
                           osal_tick_type_t timeout_ms);

/* ------------------------------ 消息队列 ---------------------------- */
osal_status_t os_queue_create(size_t queue_depth,
                              size_t data_size,
                              osal_queue_handle_t *queue_handle);
osal_status_t os_queue_delete(osal_queue_handle_t queue_handle);
osal_status_t os_queue_send(osal_queue_handle_t queue_handle,
                            const void *data,
                            osal_tick_type_t timeout_ms);
osal_status_t os_queue_receive(osal_queue_handle_t queue_handle,
                               void *data,
                               osal_tick_type_t timeout_ms);
osal_status_t os_queue_peek(osal_queue_handle_t queue_handle,
                            void *data,
                            osal_tick_type_t timeout_ms);
osal_status_t os_queue_count(osal_queue_handle_t queue_handle,
                             uint32_t *count);
osal_status_t os_queue_reset(osal_queue_handle_t queue_handle);

/* ------------------------------ 软件定时器 -------------------------- */
osal_status_t os_timer_create(osal_timer_handle_t *timer_handle,
                              const char *timer_name,
                              osal_tick_type_t timer_period_ms,
                              uint8_t auto_reload,
                              osal_timer_cb_function_t timer_cb,
                              void *arg);
osal_status_t os_timer_start(osal_timer_handle_t timer_handle,
                             osal_tick_type_t timeout_ms);
osal_status_t os_timer_stop(osal_timer_handle_t timer_handle,
                            osal_tick_type_t timeout_ms);
osal_status_t os_timer_period_change(osal_timer_handle_t timer_handle,
                                     osal_tick_type_t new_period_ms,
                                     osal_tick_type_t timeout_ms);
osal_status_t os_timer_reset(osal_timer_handle_t timer_handle,
                             osal_tick_type_t timeout_ms);
osal_status_t os_timer_delete(osal_timer_handle_t timer_handle,
                              osal_tick_type_t timeout_ms);
osal_tick_type_t os_timer_period_get(osal_timer_handle_t timer_handle);

/* ------------------------------ 任务与时间 -------------------------- */
osal_status_t os_task_create(const char *task_name,
                             osal_task_entry entry,
                             size_t stack_size,
                             osal_priority_t priority,
                             osal_task_handle_t *task_handle,
                             void *argument);
osal_status_t os_task_delete(osal_task_handle_t task_handle);
osal_status_t os_task_start(void);
osal_status_t os_task_suspend(osal_task_handle_t task_handle);
osal_status_t os_task_resume(osal_task_handle_t task_handle);
osal_status_t os_task_suspend_all(void);
osal_status_t os_task_delay(uint32_t ticks);
osal_status_t os_task_delay_ms(uint32_t delay_ms);
osal_status_t os_task_wait_until(osal_tick_type_t *last_wake,
                                 uint32_t period_ms);
osal_status_t os_critical_enter(void);
osal_status_t os_critical_exit(void);
osal_status_t os_task_yield(void);
osal_status_t os_irq_enable(void);
osal_status_t os_irq_disable(void);
osal_tick_type_t os_task_get_tick(void);
uint32_t os_task_elapsed_ms(osal_tick_type_t now, osal_tick_type_t old);
uint32_t os_task_now_ms(void);
osal_task_handle_t os_task_get_current(void);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_OS_SRC_OS_INTERNAL_H */
