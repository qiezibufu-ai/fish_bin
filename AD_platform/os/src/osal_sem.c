/**
 * @file osal_sem.c
 * @brief 实现 OSAL 二值与计数信号量稳定接口的参数检查与转发。
 */
#include "osal_sem.h"

#include "os_internal.h"

osal_status_t osal_sema_binary_create(osal_sema_handle_t *sema_handle)
{
    if (sema_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    *sema_handle = NULL;
    return os_sema_binary_create(sema_handle);
}

osal_status_t osal_sema_counting_create(osal_sema_handle_t *sema_handle,
                                        uint32_t max_count,
                                        uint32_t init_count)
{
    if (sema_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    *sema_handle = NULL;
    if (max_count == 0U || init_count > max_count)
    {
        return OSAL_ERR_PARAM;
    }
    return os_sema_counting_create(sema_handle, max_count, init_count);
}

osal_status_t osal_sema_delete(osal_sema_handle_t sema_handle)
{
    if (sema_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    return os_sema_delete(sema_handle);
}

osal_status_t osal_sema_give(osal_sema_handle_t sema_handle)
{
    if (sema_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    return os_sema_give(sema_handle);
}

osal_status_t osal_sema_take(osal_sema_handle_t sema_handle,
                             osal_tick_type_t timeout_ms)
{
    if (sema_handle == NULL)
    {
        return OSAL_ERR_PARAM;
    }
    return os_sema_take(sema_handle, timeout_ms);
}
