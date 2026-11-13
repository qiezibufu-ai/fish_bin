/**
 * @file os_freertos_diagnostics.c
 * @brief 实现基于 ESP-IDF FreeRTOS 的堆余量与任务栈水位诊断接口。
 */
#include "osal_diagnostics.h"

#include "os_freertos_internal.h"

osal_status_t osal_runtime_info(osal_runtime_info_t *out)
{
    if (out == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    *out = (osal_runtime_info_t){
        .free_heap_bytes = xPortGetFreeHeapSize(),
        .minimum_free_heap_bytes = xPortGetMinimumEverFreeHeapSize(),
        .task_count = (uint32_t)uxTaskGetNumberOfTasks()
    };
    return OSAL_OK;
}

size_t osal_task_min_stack_bytes(osal_task_handle_t handle)
{
    if (handle == NULL || os_freertos_in_isr() != 0U)
    {
        return 0U;
    }
    return (size_t)uxTaskGetStackHighWaterMark((TaskHandle_t)handle) * sizeof(StackType_t);
}
