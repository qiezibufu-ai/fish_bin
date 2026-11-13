/**
 * @file app_main.h
 * @brief 定义应用层组合根初始化、统一任务注册与系统状态查询接口。
 */
#ifndef AA_APP_APP_MAIN_H
#define AA_APP_APP_MAIN_H

#include "fw_error.h"
#include "osal_task.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 应用层组合根主生命周期状态枚举，仅 APP_STATE_FAULT 表示终态错误。 */
typedef enum
{
    APP_STATE_UNINITIALIZED = 0, /**< 尚未初始化。 */
    APP_STATE_PLATFORM_READY,    /**< 平台端口已就绪，待注册任务。 */
    APP_STATE_RUNNING,           /**< 平台与基础任务已全部就绪运行。 */
    APP_STATE_FAULT              /**< 初始化或运行发生故障，已保留首个错误快照。 */
} app_state_t;

/** @brief 应用层错误定位中的操作阶段枚举。 */
typedef enum
{
    APP_STAGE_GPIO_INIT = 1, /**< 初始化板级 GPIO 端口阶段。 */
    APP_STAGE_UART_INIT,     /**< 初始化板级 UART 端口阶段。 */
    APP_STAGE_TASK_REGISTER, /**< 统一注册应用任务阶段。 */
    APP_STAGE_VALIDATE,      /**< 内部状态机合法性校验阶段。 */
    APP_STAGE_MOTOR_BIND     /**< 绑定电机驱动，不发送运动命令。 */
} app_stage_t;

/**
 * @brief 初始化底层平台端口（GPIO、单路电机 UART 与协议绑定），不创建后台任务。
 * @retval FW_OK 平台端口全部就绪，状态进入 APP_STATE_PLATFORM_READY。
 * @retval FW_ERR_APP_ALREADY_INIT 应用已完成初始化。
 * @retval FW_ERR_APP_PLATFORM_INIT_FAILED 某个平台端口初始化失败，底层源头保存在错误快照。
 * @return 若已处于 APP_STATE_FAULT，原样返回首个故障码。
 * @note 仅任务上下文调用；失败时自动回收已初始化的平台端口资源，不隐式使能任何执行器。
 */
fw_error_t app_init_platform(void);

/**
 * @brief 执行完整应用组合根初始化（依次准备平台端口并注册基础监控任务）。
 * @retval FW_OK 平台端口与基础任务全部初始化成功，状态进入 APP_STATE_RUNNING。
 * @retval FW_ERR_APP_ALREADY_INIT 应用已处于运行状态。
 * @retval FW_ERR_APP_PLATFORM_INIT_FAILED 平台端口初始化失败。
 * @retval FW_ERR_APP_TASK_CREATE_FAILED 基础任务创建失败。
 * @return 若已处于 APP_STATE_FAULT，原样返回首个故障码。
 * @note 仅在 ESP-IDF app_main() 启动上下文调用一次。
 */
fw_error_t app_init(void);

/**
 * @brief 反初始化应用层，停止基础任务并释放平台端口资源。
 * @retval FW_OK 反初始化成功，状态回到 APP_STATE_UNINITIALIZED。
 * @note 仅启动/维护上下文调用；调用方必须先在电机所有者任务内取消并确认停机，
 * 再停止该任务后调用本函数；不能与bootstrap事务并发，允许FAULT后显式恢复。
 */
fw_error_t app_deinit(void);

/**
 * @brief 读取应用层当前主状态与首个失败快照（含底层错误源头与清理错误码）。
 * @param[out] state 输出当前应用主状态，不得为NULL。
 * @param[out] error 输出完整错误快照，不得为NULL。
 * @retval FW_OK 快照复制成功。
 * @retval FW_ERR_APP_INVALID_ARG state 或 error 为NULL。
 * @note 不阻塞，不依赖串口打印。
 */
fw_error_t app_get_status(app_state_t *state, fw_error_record_t *error);

#ifdef __cplusplus
}
#endif
#endif /* AA_APP_APP_MAIN_H */
