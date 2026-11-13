/**
 * @file os_freertos_task.c
 * @brief 实现基于 ESP-IDF FreeRTOS 的任务、节拍与跨核临界区后端。
 */
#include "os_freertos_internal.h"

/** @brief 供 osal_enter_critical / osal_exit_critical 使用的 ESP-IDF SMP 自旋锁。 */
static portMUX_TYPE s_osal_critical_mux = portMUX_INITIALIZER_UNLOCKED;

/** @brief 校验任务名长度不超过 FreeRTOS 配置上限。 */
static bool freertos_task_name_valid(const char *name, size_t capacity)
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

osal_status_t os_task_create(const char *task_name,
                             osal_task_entry entry,
                             size_t stack_size,
                             osal_priority_t priority,
                             osal_task_handle_t *task_handle,
                             void *argument)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    if (!freertos_task_name_valid(task_name, (size_t)configMAX_TASK_NAME_LEN) ||
        priority >= (osal_priority_t)configMAX_PRIORITIES)
    {
        return OSAL_ERR_PARAM;
    }

    size_t stack_depth = stack_size / sizeof(StackType_t);
    if ((stack_size % sizeof(StackType_t)) != 0U)
    {
        stack_depth++;
    }
    if (stack_depth == 0U || stack_depth > (size_t)((configSTACK_DEPTH_TYPE)-1))
    {
        return OSAL_ERR_PARAM;
    }

    TaskHandle_t handle = NULL;
    BaseType_t status = xTaskCreate(entry,
                                    task_name,
                                    (configSTACK_DEPTH_TYPE)stack_depth,
                                    argument,
                                    (UBaseType_t)priority,
                                    (task_handle != NULL) ? &handle : NULL);
    if (status != pdPASS)
    {
        return OSAL_ERR_NO_MEMORY;
    }

    if (task_handle != NULL)
    {
        *task_handle = (osal_task_handle_t)handle;
    }
    return OSAL_OK;
}

osal_status_t os_task_delete(osal_task_handle_t task_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    vTaskDelete((TaskHandle_t)task_handle);
    return OSAL_OK;
}

osal_status_t os_task_start(void)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        return OSAL_OK;
    }
    vTaskStartScheduler();
    return OSAL_ERR_NO_MEMORY;
}

osal_status_t os_task_suspend(osal_task_handle_t task_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    vTaskSuspend((TaskHandle_t)task_handle);
    return OSAL_OK;
}

osal_status_t os_task_resume(osal_task_handle_t task_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        BaseType_t task_woken = xTaskResumeFromISR((TaskHandle_t)task_handle);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        vTaskResume((TaskHandle_t)task_handle);
    }
    return OSAL_OK;
}

osal_status_t os_task_suspend_all(void)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    vTaskSuspendAll();
    return OSAL_OK;
}

osal_status_t os_task_delay(uint32_t ticks)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    vTaskDelay((TickType_t)ticks);
    return OSAL_OK;
}

osal_status_t os_task_delay_ms(uint32_t delay_ms)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    vTaskDelay(os_freertos_ms_ticks((osal_tick_type_t)delay_ms));
    return OSAL_OK;
}

osal_status_t os_task_wait_until(osal_tick_type_t *last_wake,
                                 uint32_t period_ms)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    TickType_t wake_tick = (TickType_t)(*last_wake);
    TickType_t period_ticks = os_freertos_ms_ticks((osal_tick_type_t)period_ms);
    (void)xTaskDelayUntil(&wake_tick, period_ticks);
    *last_wake = (osal_tick_type_t)wake_tick;
    return OSAL_OK;
}

osal_status_t os_critical_enter(void)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    portENTER_CRITICAL(&s_osal_critical_mux);
    return OSAL_OK;
}

osal_status_t os_critical_exit(void)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    portEXIT_CRITICAL(&s_osal_critical_mux);
    return OSAL_OK;
}

osal_status_t os_task_yield(void)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    portYIELD();
    return OSAL_OK;
}

osal_status_t os_irq_enable(void)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    portENABLE_INTERRUPTS();
    return OSAL_OK;
}

osal_status_t os_irq_disable(void)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    portDISABLE_INTERRUPTS();
    return OSAL_OK;
}

osal_tick_type_t os_task_get_tick(void)
{
    TickType_t ticks;
    if (os_freertos_in_isr() != 0U)
    {
        ticks = xTaskGetTickCountFromISR();
    }
    else
    {
        ticks = xTaskGetTickCount();
    }
    return (osal_tick_type_t)ticks;
}

osal_task_handle_t os_task_get_current(void)
{
    return (osal_task_handle_t)xTaskGetCurrentTaskHandle();
}

uint32_t os_task_elapsed_ms(osal_tick_type_t now, osal_tick_type_t old)
{
    int32_t delta = (int32_t)((TickType_t)now - (TickType_t)old);
    if (delta <= 0)
    {
        return 0U;
    }
    return os_freertos_ticks_to_ms((TickType_t)delta);
}

uint32_t os_task_now_ms(void)
{
    return os_freertos_ticks_to_ms((TickType_t)os_task_get_tick());
}
