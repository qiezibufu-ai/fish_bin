/**
 * @file osal_task.c
 * @brief 实现 OSAL 任务与时间接口的参数检查，并转发至底层 RTOS 实现。
 */
#include "osal_task.h"

#include "os_internal.h"

/** @brief 校验名称非空且在容量上限内包含结尾空字符。 */
static bool task_name_valid(const char *name, size_t capacity)
{
    if (name == NULL || name[0] == '\0')
    {
        return false;
    }
    for (size_t i = 0U; i < capacity; ++i)
    {
        if (name[i] == '\0')
        {
            return true;
        }
    }
    return false;
}

osal_status_t osal_task_create(const char *task_name,
                               osal_task_entry entry,
                               size_t stack_size,
                               osal_priority_t priority,
                               osal_task_handle_t *task_handle,
                               void *argument)
{
    if (task_handle != NULL)
    {
        *task_handle = NULL;
    }
    if (task_name == NULL || entry == NULL || stack_size == 0U)
    {
        return OSAL_ERR_PARAM;
    }
    if (!task_name_valid(task_name, OSAL_NAME_MAX_LEN))
    {
        return OSAL_ERR_PARAM;
    }
    return os_task_create(task_name, entry, stack_size, priority, task_handle, argument);
}

osal_status_t osal_task_delete(osal_task_handle_t task_handle)
{
    return os_task_delete(task_handle);
}

osal_status_t osal_task_start(void)
{
    return os_task_start();
}

osal_status_t osal_task_suspend(osal_task_handle_t task_handle)
{
    return os_task_suspend(task_handle);
}

osal_status_t osal_task_resume(osal_task_handle_t task_handle)
{
    if (task_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    return os_task_resume(task_handle);
}

osal_status_t osal_task_suspend_all(void)
{
    return os_task_suspend_all();
}

osal_status_t osal_task_delay(uint32_t ticks)
{
    return os_task_delay(ticks);
}

osal_status_t osal_task_delay_ms(uint32_t delay_ms)
{
    if (delay_ms == OSAL_MAX_DELAY)
    {
        return OSAL_ERR_PARAM;
    }
    return os_task_delay_ms(delay_ms);
}

osal_status_t osal_task_wait_until(osal_tick_type_t *last_wake,
                                   uint32_t period_ms)
{
    if (last_wake == NULL || period_ms == 0U || period_ms == OSAL_MAX_DELAY)
    {
        return OSAL_ERR_PARAM;
    }
    return os_task_wait_until(last_wake, period_ms);
}

osal_status_t osal_enter_critical(void)
{
    return os_critical_enter();
}

osal_status_t osal_exit_critical(void)
{
    return os_critical_exit();
}

osal_status_t osal_port_yield(void)
{
    return os_task_yield();
}

osal_status_t osal_task_enable_interrupts(void)
{
    return os_irq_enable();
}

osal_status_t osal_task_disable_interrupts(void)
{
    return os_irq_disable();
}

osal_tick_type_t osal_task_get_tick_count(void)
{
    return os_task_get_tick();
}

uint32_t osal_task_elapsed_ms(osal_tick_type_t now, osal_tick_type_t old)
{
    return os_task_elapsed_ms(now, old);
}

uint32_t osal_task_now_ms(void)
{
    return os_task_now_ms();
}

osal_task_handle_t osal_task_get_current_handle(void)
{
    return os_task_get_current();
}
