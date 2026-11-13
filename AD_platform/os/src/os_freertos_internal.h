/**
 * @file os_freertos_internal.h
 * @brief 定义 ESP-IDF FreeRTOS 后端的公共内联工具与编译期约束检查。
 */
#ifndef AD_PLATFORM_OS_SRC_OS_FREERTOS_INTERNAL_H
#define AD_PLATFORM_OS_SRC_OS_FREERTOS_INTERNAL_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "os_internal.h"

#if (configSUPPORT_DYNAMIC_ALLOCATION != 1)
#error "OSAL requires FreeRTOS dynamic allocation"
#endif

#if (configUSE_MUTEXES != 1)
#error "OSAL mutex support requires configUSE_MUTEXES"
#endif

#if (configUSE_COUNTING_SEMAPHORES != 1)
#error "OSAL counting semaphore support requires configUSE_COUNTING_SEMAPHORES"
#endif

#if (configUSE_TIMERS != 1)
#error "OSAL software timer support requires configUSE_TIMERS"
#endif

#if (configUSE_16_BIT_TICKS != 0)
#error "OSAL public tick type requires 32-bit FreeRTOS TickType_t"
#endif

/**
 * @brief 判断当前 CPU 核是否运行在中断服务（ISR）上下文中。
 * @return 1U 表示处于 ISR 上下文，0U 表示处于任务上下文。
 */
static inline uint8_t os_freertos_in_isr(void)
{
    return (xPortInIsrContext() != pdFALSE) ? 1U : 0U;
}

/**
 * @brief 将毫秒转换为 FreeRTOS TickType_t，非零毫秒向上取整防止小延时截断为0。
 * @param[in] time_ms 输入毫秒数；OSAL_MAX_DELAY 映射为 portMAX_DELAY。
 * @return 对应的 FreeRTOS 节拍数。
 */
static inline TickType_t os_freertos_ms_ticks(osal_tick_type_t time_ms)
{
    if (time_ms == OSAL_MAX_DELAY)
    {
        return portMAX_DELAY;
    }
    if (time_ms == 0U)
    {
        return (TickType_t)0U;
    }
    uint64_t ticks = ((uint64_t)time_ms * (uint64_t)configTICK_RATE_HZ + 999ULL) / 1000ULL;
    if (ticks == 0ULL)
    {
        ticks = 1ULL;
    }
    if (ticks >= (uint64_t)portMAX_DELAY)
    {
        ticks = (uint64_t)portMAX_DELAY - 1ULL;
    }
    return (TickType_t)ticks;
}

/**
 * @brief 将 FreeRTOS TickType_t 转换为毫秒数。
 * @param[in] ticks FreeRTOS 节拍数。
 * @return 对应的毫秒数，单位ms。
 */
static inline uint32_t os_freertos_ticks_to_ms(TickType_t ticks)
{
    uint64_t ms = ((uint64_t)ticks * 1000ULL) / (uint64_t)configTICK_RATE_HZ;
    return (ms > (uint64_t)UINT32_MAX) ? UINT32_MAX : (uint32_t)ms;
}

#endif /* AD_PLATFORM_OS_SRC_OS_FREERTOS_INTERNAL_H */
