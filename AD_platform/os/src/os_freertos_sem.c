/**
 * @file os_freertos_sem.c
 * @brief 实现基于 ESP-IDF FreeRTOS 的二值与计数信号量后端（自动区分任务与 ISR）。
 */
#include "os_freertos_internal.h"

osal_status_t os_sema_binary_create(osal_sema_handle_t *sema_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    SemaphoreHandle_t handle = xSemaphoreCreateBinary();
    if (handle == NULL)
    {
        return OSAL_ERR_NO_MEMORY;
    }
    *sema_handle = (osal_sema_handle_t)handle;
    return OSAL_OK;
}

osal_status_t os_sema_counting_create(osal_sema_handle_t *sema_handle,
                                      uint32_t max_count,
                                      uint32_t init_count)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    if (max_count > (uint32_t)((UBaseType_t)-1))
    {
        return OSAL_ERR_PARAM;
    }
    SemaphoreHandle_t handle = xSemaphoreCreateCounting((UBaseType_t)max_count,
                                                        (UBaseType_t)init_count);
    if (handle == NULL)
    {
        return OSAL_ERR_NO_MEMORY;
    }
    *sema_handle = (osal_sema_handle_t)handle;
    return OSAL_OK;
}

osal_status_t os_sema_delete(osal_sema_handle_t sema_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    vSemaphoreDelete((SemaphoreHandle_t)sema_handle);
    return OSAL_OK;
}

osal_status_t os_sema_give(osal_sema_handle_t sema_handle)
{
    SemaphoreHandle_t handle = (SemaphoreHandle_t)sema_handle;
    BaseType_t status;

    if (os_freertos_in_isr() != 0U)
    {
        BaseType_t task_woken = pdFALSE;
        status = xSemaphoreGiveFromISR(handle, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        status = xSemaphoreGive(handle);
    }
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_STATE;
}

osal_status_t os_sema_take(osal_sema_handle_t sema_handle,
                           osal_tick_type_t timeout_ms)
{
    SemaphoreHandle_t handle = (SemaphoreHandle_t)sema_handle;
    BaseType_t status;

    if (os_freertos_in_isr() != 0U)
    {
        BaseType_t task_woken = pdFALSE;
        status = xSemaphoreTakeFromISR(handle, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        status = xSemaphoreTake(handle, os_freertos_ms_ticks(timeout_ms));
    }
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_TIMEOUT;
}
