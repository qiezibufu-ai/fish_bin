/**
 * @file osal_timer.c
 * @brief 实现 OSAL 软件定时器稳定接口的参数检查与转发。
 */
#include "osal_timer.h"

#include "os_internal.h"

/** @brief 校验定时器名称非空且在最大长度上限内包含结尾空字符。 */
static bool timer_name_valid(const char *name, size_t capacity)
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

osal_status_t osal_timer_create(osal_timer_handle_t *timer_handle,
                                const char *timer_name,
                                osal_tick_type_t timer_period_ms,
                                uint8_t auto_reload,
                                osal_timer_cb_function_t timer_cb,
                                void *arg)
{
    if (timer_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    *timer_handle = NULL;
    if (timer_cb == NULL || timer_period_ms == 0U || timer_period_ms == OSAL_MAX_DELAY)
    {
        return OSAL_ERR_PARAM;
    }
    if (!timer_name_valid(timer_name, OSAL_NAME_MAX_LEN))
    {
        return OSAL_ERR_PARAM;
    }
    return os_timer_create(timer_handle, timer_name, timer_period_ms,
                           auto_reload, timer_cb, arg);
}

osal_status_t osal_timer_start(osal_timer_handle_t timer_handle,
                               osal_tick_type_t timeout_ms)
{
    if (timer_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    return os_timer_start(timer_handle, timeout_ms);
}

osal_status_t osal_timer_stop(osal_timer_handle_t timer_handle,
                              osal_tick_type_t timeout_ms)
{
    if (timer_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    return os_timer_stop(timer_handle, timeout_ms);
}

osal_status_t osal_timer_period_change(osal_timer_handle_t timer_handle,
                                       osal_tick_type_t new_period_ms,
                                       osal_tick_type_t timeout_ms)
{
    if (timer_handle == NULL || new_period_ms == 0U || new_period_ms == OSAL_MAX_DELAY)
    {
        return OSAL_ERR_PARAM;
    }
    return os_timer_period_change(timer_handle, new_period_ms, timeout_ms);
}

osal_status_t osal_timer_reset(osal_timer_handle_t timer_handle,
                               osal_tick_type_t timeout_ms)
{
    if (timer_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    return os_timer_reset(timer_handle, timeout_ms);
}

osal_status_t osal_timer_delete(osal_timer_handle_t timer_handle,
                                osal_tick_type_t timeout_ms)
{
    if (timer_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    return os_timer_delete(timer_handle, timeout_ms);
}

osal_tick_type_t osal_timer_period_get(osal_timer_handle_t timer_handle)
{
    if (timer_handle == NULL)
    {
        return 0U;
    }
    return os_timer_period_get(timer_handle);
}
