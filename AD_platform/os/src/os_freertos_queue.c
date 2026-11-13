/**
 * @file os_freertos_queue.c
 * @brief 实现基于 ESP-IDF FreeRTOS 的消息队列后端（自动区分任务与 ISR）。
 */
#include "os_freertos_internal.h"

osal_status_t os_queue_create(size_t queue_depth,
                              size_t data_size,
                              osal_queue_handle_t *queue_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    if (queue_depth > (size_t)((UBaseType_t)-1) ||
        data_size > (size_t)((UBaseType_t)-1))
    {
        return OSAL_ERR_PARAM;
    }
    QueueHandle_t handle = xQueueCreate((UBaseType_t)queue_depth,
                                        (UBaseType_t)data_size);
    if (handle == NULL)
    {
        return OSAL_ERR_NO_MEMORY;
    }
    *queue_handle = (osal_queue_handle_t)handle;
    return OSAL_OK;
}

osal_status_t os_queue_delete(osal_queue_handle_t queue_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    vQueueDelete((QueueHandle_t)queue_handle);
    return OSAL_OK;
}

osal_status_t os_queue_send(osal_queue_handle_t queue_handle,
                            const void *data,
                            osal_tick_type_t timeout_ms)
{
    QueueHandle_t handle = (QueueHandle_t)queue_handle;
    BaseType_t status;

    if (os_freertos_in_isr() != 0U)
    {
        BaseType_t task_woken = pdFALSE;
        status = xQueueSendFromISR(handle, data, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        status = xQueueSend(handle, data, os_freertos_ms_ticks(timeout_ms));
    }
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_BUSY;
}

osal_status_t os_queue_receive(osal_queue_handle_t queue_handle,
                               void *data,
                               osal_tick_type_t timeout_ms)
{
    QueueHandle_t handle = (QueueHandle_t)queue_handle;
    BaseType_t status;

    if (os_freertos_in_isr() != 0U)
    {
        BaseType_t task_woken = pdFALSE;
        status = xQueueReceiveFromISR(handle, data, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        status = xQueueReceive(handle, data, os_freertos_ms_ticks(timeout_ms));
    }
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_TIMEOUT;
}

osal_status_t os_queue_peek(osal_queue_handle_t queue_handle,
                            void *data,
                            osal_tick_type_t timeout_ms)
{
    QueueHandle_t handle = (QueueHandle_t)queue_handle;
    BaseType_t status;

    if (os_freertos_in_isr() != 0U)
    {
        status = xQueuePeekFromISR(handle, data);
    }
    else
    {
        status = xQueuePeek(handle, data, os_freertos_ms_ticks(timeout_ms));
    }
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_TIMEOUT;
}

osal_status_t os_queue_count(osal_queue_handle_t queue_handle,
                             uint32_t *count)
{
    QueueHandle_t handle = (QueueHandle_t)queue_handle;
    UBaseType_t native_count;

    if (os_freertos_in_isr() != 0U)
    {
        native_count = uxQueueMessagesWaitingFromISR(handle);
    }
    else
    {
        native_count = uxQueueMessagesWaiting(handle);
    }
    if (native_count > (UBaseType_t)UINT32_MAX)
    {
        return OSAL_ERR_FAIL;
    }
    *count = (uint32_t)native_count;
    return OSAL_OK;
}

osal_status_t os_queue_reset(osal_queue_handle_t queue_handle)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    (void)xQueueReset((QueueHandle_t)queue_handle);
    return OSAL_OK;
}
