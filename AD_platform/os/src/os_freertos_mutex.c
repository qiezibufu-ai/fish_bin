/**
 * @file os_freertos_mutex.c
 * @brief 实现基于 ESP-IDF FreeRTOS 的互斥锁后端。
 */
#include "os_freertos_internal.h"

osal_status_t os_mutex_create(osal_mutex_handle_t *mutex_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    SemaphoreHandle_t handle = xSemaphoreCreateMutex();
    if (handle == NULL)
    {
        return OSAL_ERR_NO_MEMORY;
    }
    *mutex_handle = (osal_mutex_handle_t)handle;
    return OSAL_OK;
}

osal_status_t os_mutex_delete(osal_mutex_handle_t mutex_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    vSemaphoreDelete((SemaphoreHandle_t)mutex_handle);
    return OSAL_OK;
}

osal_status_t os_mutex_give(osal_mutex_handle_t mutex_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    BaseType_t status = xSemaphoreGive((SemaphoreHandle_t)mutex_handle);
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_STATE;
}

osal_status_t os_mutex_take(osal_mutex_handle_t mutex_handle,
                            osal_tick_type_t timeout_ms)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    BaseType_t status = xSemaphoreTake((SemaphoreHandle_t)mutex_handle,
                                       os_freertos_ms_ticks(timeout_ms));
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_TIMEOUT;
}
