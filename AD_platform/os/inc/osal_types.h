/**
 * @file osal_types.h
 * @brief 定义 OSAL 公共基础类型、常量与不透明内核资源句柄。
 */
#ifndef AD_PLATFORM_OS_INC_OSAL_TYPES_H
#define AD_PLATFORM_OS_INC_OSAL_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 无限等待超时常量，仅允许在支持无限等待的同步原语中使用。 */
#define OSAL_MAX_DELAY (UINT32_MAX)

/** @brief 任务与定时器名称包含结尾'\0'的最大字节长度。 */
#define OSAL_NAME_MAX_LEN (16U)

/** @brief OSAL 统一时间与节拍类型（公开接口中超时参数按注释以 ms 或 tick 为单位）。 */
typedef uint32_t osal_tick_type_t;

/** @brief 与具体 RTOS 解耦的任务数值优先级类型（数值越大优先级越高）。 */
typedef uint32_t osal_priority_t;

/** @brief 不透明任务句柄类型。 */
typedef struct osal_task_obj *osal_task_handle_t;
/** @brief 不透明信号量句柄类型。 */
typedef struct osal_sema_obj *osal_sema_handle_t;
/** @brief 不透明互斥锁句柄类型。 */
typedef struct osal_mutex_obj *osal_mutex_handle_t;
/** @brief 不透明消息队列句柄类型。 */
typedef struct osal_queue_obj *osal_queue_handle_t;
/** @brief 不透明软件定时器句柄类型。 */
typedef struct osal_timer_obj *osal_timer_handle_t;

/**
 * @brief OSAL 任务入口函数指针类型。
 * @param[in] argument 创建任务时传入的用户上下文指针，可为NULL。
 * @note 任务入口运行在独立任务上下文，退出前应自行调用 osal_task_delete(NULL) 或保持循环。
 */
typedef void (*osal_task_entry)(void *argument);

/**
 * @brief OSAL 软件定时器到期回调函数指针类型。
 * @param[in] timer_handle 触发本次回调的定时器句柄。
 * @param[in] argument 创建定时器时绑定的用户上下文指针，可为NULL。
 * @note 运行在 RTOS 定时器服务任务上下文，禁止在回调内阻塞、休眠或等待互斥锁。
 */
typedef void (*osal_timer_cb_function_t)(osal_timer_handle_t timer_handle,
                                         void *argument);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_OS_INC_OSAL_TYPES_H */
