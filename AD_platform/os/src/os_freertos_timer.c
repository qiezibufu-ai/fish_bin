/**
 * @file os_freertos_timer.c
 * @brief 实现基于 ESP-IDF FreeRTOS 的软件定时器后端，保证删除时释放上下文内存。
 */
#include "os_freertos_internal.h"

/** @brief 内部软件定时器对象，绑定 FreeRTOS 定时器句柄与用户回调上下文。 */
struct osal_timer_obj
{
    TimerHandle_t handle;          /**< FreeRTOS 原生软件定时器句柄。 */
    osal_timer_cb_function_t cb;   /**< 用户到期回调函数。 */
    void *arg;                     /**< 用户上下文参数。 */
};

/** @brief FreeRTOS 软件定时器统一回调桥接入口。 */
static void freertos_timer_bridge_cb(TimerHandle_t xTimer)
{
    struct osal_timer_obj *obj = (struct osal_timer_obj *)pvTimerGetTimerID(xTimer);
    if (obj != NULL && obj->cb != NULL)
    {
        obj->cb((osal_timer_handle_t)obj, obj->arg);
    }
}

osal_status_t os_timer_create(osal_timer_handle_t *timer_handle,
                              const char *timer_name,
                              osal_tick_type_t timer_period_ms,
                              uint8_t auto_reload,
                              osal_timer_cb_function_t timer_cb,
                              void *arg)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    struct osal_timer_obj *obj =
        (struct osal_timer_obj *)pvPortMalloc(sizeof(struct osal_timer_obj));
    if (obj == NULL)
    {
        return OSAL_ERR_NO_MEMORY;
    }
    obj->cb = timer_cb;
    obj->arg = arg;
    obj->handle = xTimerCreate(timer_name,
                               os_freertos_ms_ticks(timer_period_ms),
                               (auto_reload != 0U) ? pdTRUE : pdFALSE,
                               (void *)obj,
                               freertos_timer_bridge_cb);
    if (obj->handle == NULL)
    {
        vPortFree(obj);
        return OSAL_ERR_NO_MEMORY;
    }
    *timer_handle = (osal_timer_handle_t)obj;
    return OSAL_OK;
}

osal_status_t os_timer_start(osal_timer_handle_t timer_handle,
                             osal_tick_type_t timeout_ms)
{
    struct osal_timer_obj *obj = (struct osal_timer_obj *)timer_handle;
    BaseType_t status;

    if (os_freertos_in_isr() != 0U)
    {
        BaseType_t task_woken = pdFALSE;
        status = xTimerStartFromISR(obj->handle, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        status = xTimerStart(obj->handle, os_freertos_ms_ticks(timeout_ms));
    }
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_TIMEOUT;
}

osal_status_t os_timer_stop(osal_timer_handle_t timer_handle,
                            osal_tick_type_t timeout_ms)
{
    struct osal_timer_obj *obj = (struct osal_timer_obj *)timer_handle;
    BaseType_t status;

    if (os_freertos_in_isr() != 0U)
    {
        BaseType_t task_woken = pdFALSE;
        status = xTimerStopFromISR(obj->handle, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        status = xTimerStop(obj->handle, os_freertos_ms_ticks(timeout_ms));
    }
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_TIMEOUT;
}

osal_status_t os_timer_period_change(osal_timer_handle_t timer_handle,
                                     osal_tick_type_t new_period_ms,
                                     osal_tick_type_t timeout_ms)
{
    struct osal_timer_obj *obj = (struct osal_timer_obj *)timer_handle;
    TickType_t new_ticks = os_freertos_ms_ticks(new_period_ms);
    BaseType_t status;

    if (os_freertos_in_isr() != 0U)
    {
        BaseType_t task_woken = pdFALSE;
        status = xTimerChangePeriodFromISR(obj->handle, new_ticks, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        status = xTimerChangePeriod(obj->handle, new_ticks,
                                    os_freertos_ms_ticks(timeout_ms));
    }
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_TIMEOUT;
}

osal_status_t os_timer_reset(osal_timer_handle_t timer_handle,
                             osal_tick_type_t timeout_ms)
{
    struct osal_timer_obj *obj = (struct osal_timer_obj *)timer_handle;
    BaseType_t status;

    if (os_freertos_in_isr() != 0U)
    {
        BaseType_t task_woken = pdFALSE;
        status = xTimerResetFromISR(obj->handle, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    }
    else
    {
        status = xTimerReset(obj->handle, os_freertos_ms_ticks(timeout_ms));
    }
    return (status == pdPASS) ? OSAL_OK : OSAL_ERR_TIMEOUT;
}

osal_status_t os_timer_delete(osal_timer_handle_t timer_handle,
                              osal_tick_type_t timeout_ms)
{
    if (os_freertos_in_isr() != 0U)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    struct osal_timer_obj *obj = (struct osal_timer_obj *)timer_handle;
    TimerHandle_t raw_handle = obj->handle;

    /* 先解绑 TimerID，防止命令队列处理删除前晚到触发的回调解引用已释放内存。 */
    vTimerSetTimerID(raw_handle, NULL);
    BaseType_t status = xTimerDelete(raw_handle, os_freertos_ms_ticks(timeout_ms));
    if (status != pdPASS)
    {
        vTimerSetTimerID(raw_handle, (void *)obj);
        return OSAL_ERR_TIMEOUT;
    }
    obj->cb = NULL;
    obj->handle = NULL;
    vPortFree(obj);
    return OSAL_OK;
}

osal_tick_type_t os_timer_period_get(osal_timer_handle_t timer_handle)
{
    struct osal_timer_obj *obj = (struct osal_timer_obj *)timer_handle;
    if (obj == NULL || obj->handle == NULL)
    {
        return 0U;
    }
    TickType_t ticks = xTimerGetPeriod(obj->handle);
    return (osal_tick_type_t)os_freertos_ticks_to_ms(ticks);
}
