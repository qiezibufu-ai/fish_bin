/**
 * @file gpio_port.h
 * @brief 定义 ESP32-S3 GPIO 平台端口接口、逻辑引脚枚举与可定位错误状态查询。
 */
#ifndef AD_PLATFORM_MCU_GPIO_INC_GPIO_PORT_H
#define AD_PLATFORM_MCU_GPIO_INC_GPIO_PORT_H

#include "fw_error.h"
#include "platform_error.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 逻辑 GPIO 引脚标识枚举，由 AF_config/board_config.h 映射物理引脚。 */
typedef enum
{
    CORE_GPIO_PIN_STATUS_LED = 0, /**< 旧状态灯逻辑编号；默认未连接，WS2812由面板端口管理。 */
    CORE_GPIO_PIN_RS485_BUS1_DE,  /**< RS485 总线1收发方向控制引脚。 */
    CORE_GPIO_PIN_RS485_BUS2_DE,  /**< RS485 总线2收发方向控制引脚。 */
    CORE_GPIO_PIN_SENSOR_TRIG,    /**< 外部传感器脉冲触发输入引脚。 */
    CORE_GPIO_PIN_MAX             /**< 逻辑引脚总数上界。 */
} core_gpio_pin_t;

/** @brief 兼容旧工程的逻辑引脚枚举别名。 */
typedef core_gpio_pin_t en_core_gpio_pin_t;

/** @brief GPIO 引脚逻辑电平枚举。 */
typedef enum
{
    CORE_GPIO_LEVEL_LOW = 0, /**< 低电平（0）。 */
    CORE_GPIO_LEVEL_HIGH = 1 /**< 高电平（1）。 */
} core_gpio_level_t;

/** @brief 兼容旧工程的引脚状态别名。 */
typedef core_gpio_level_t en_core_gpio_pin_state_t;
/** @brief 兼容旧工程低电平宏常量。 */
#define CORE_GPIO_PIN_RESET CORE_GPIO_LEVEL_LOW
/** @brief 兼容旧工程高电平宏常量。 */
#define CORE_GPIO_PIN_SET   CORE_GPIO_LEVEL_HIGH

/** @brief GPIO 引脚输入/输出方向枚举。 */
typedef enum
{
    CORE_GPIO_DIR_INPUT = 0,   /**< 纯输入模式。 */
    CORE_GPIO_DIR_OUTPUT,      /**< 推挽输出模式。 */
    CORE_GPIO_DIR_INPUT_OUTPUT /**< 输入输出双向模式（支持读回输出电平）。 */
} core_gpio_dir_t;

/** @brief GPIO 上下拉电阻配置枚举。 */
typedef enum
{
    CORE_GPIO_PULL_NONE = 0, /**< 浮空，无内部上下拉。 */
    CORE_GPIO_PULL_UP,       /**< 内部上拉使能。 */
    CORE_GPIO_PULL_DOWN      /**< 内部下拉使能。 */
} core_gpio_pull_t;

/** @brief GPIO 外部中断触发边沿枚举。 */
typedef enum
{
    CORE_GPIO_IRQ_NONE = 0, /**< 禁用引脚中断。 */
    CORE_GPIO_IRQ_RISING,   /**< 上升沿触发中断。 */
    CORE_GPIO_IRQ_FALLING,  /**< 下降沿触发中断。 */
    CORE_GPIO_IRQ_ANYEDGE   /**< 双边沿触发中断。 */
} core_gpio_irq_trig_t;

/** @brief GPIO 端口主生命周期状态枚举，仅 CORE_GPIO_STATE_FAULT 表示终态错误。 */
typedef enum
{
    CORE_GPIO_STATE_UNINITIALIZED = 0, /**< 尚未初始化或已反初始化。 */
    CORE_GPIO_STATE_READY,             /**< 初始化完成，可正常执行 IO 操作。 */
    CORE_GPIO_STATE_FAULT              /**< 发生故障，已锁存首个具体失败记录。 */
} core_gpio_state_t;

/** @brief GPIO 错误定位中的操作阶段枚举。 */
typedef enum
{
    CORE_GPIO_STAGE_INIT = 1, /**< 板级引脚批量初始化阶段。 */
    CORE_GPIO_STAGE_CONFIG,   /**< 单引脚重配置或方向切换阶段。 */
    CORE_GPIO_STAGE_IO,       /**< 读写或翻转电平阶段。 */
    CORE_GPIO_STAGE_ISR,      /**< 中断服务安装、回调绑定或使能阶段。 */
    CORE_GPIO_STAGE_DEINIT,   /**< 反初始化清理阶段。 */
    CORE_GPIO_STAGE_VALIDATE  /**< 内部状态机合法性检查阶段。 */
} core_gpio_stage_t;

/** @brief 单个逻辑 GPIO 引脚的硬件配置描述符。 */
typedef struct
{
    int32_t hw_pin_num;              /**< ESP32-S3 物理 GPIO 编号；BOARD_GPIO_PIN_NC(-1) 表示未连接。 */
    core_gpio_dir_t direction;       /**< 引脚方向。 */
    core_gpio_pull_t pull_mode;      /**< 内部上下拉模式。 */
    core_gpio_level_t default_level; /**< 输出模式下的初始安全电平。 */
} core_gpio_pin_config_t;

/**
 * @brief GPIO 引脚中断服务回调函数指针类型。
 * @param[in] pin 触发中断的逻辑引脚枚举。
 * @param[in] arg 注册回调时绑定的上下文指针，可为NULL。
 * @note 运行在 ISR 上下文，严禁阻塞、分配堆内存或获取普通互斥锁。
 */
typedef void (*core_gpio_isr_cb_t)(core_gpio_pin_t pin, void *arg);

/**
 * @brief 按 AF_config/board_config.h 默认映射初始化全部已连接板级 GPIO 引脚。
 * @retval FW_OK 全部已连接引脚配置完成，状态进入 CORE_GPIO_STATE_READY。
 * @retval FW_ERR_GPIO_CONFIG_FAILED 底层引脚配置失败，原生错误码保存在错误快照。
 * @note 仅任务上下文调用；支持从 UNINITIALIZED 或 FAULT 状态显式重新初始化。
 */
fw_error_t core_gpio_init(void);

/**
 * @brief 反初始化 GPIO 端口，移除已注册中断并将输出引脚恢复为默认安全电平。
 * @retval FW_OK 反初始化完成，状态回到 CORE_GPIO_STATE_UNINITIALIZED。
 * @retval FW_ERR_GPIO_IO_FAILED 复位底层引脚失败，详情保存在错误快照。
 * @note 仅任务上下文调用。
 */
fw_error_t core_gpio_deinit(void);

/**
 * @brief 单独配置或重配置指定逻辑 GPIO 引脚。
 * @param[in] pin 逻辑引脚标识，范围 0..(CORE_GPIO_PIN_MAX-1)。
 * @param[in] config 引脚配置参数，不得为NULL，hw_pin_num 必须为有效非负引脚号。
 * @retval FW_OK 引脚配置成功。
 * @retval FW_ERR_GPIO_INVALID_ARG pin 越界、config 为NULL或枚举值非法。
 * @retval FW_ERR_GPIO_NOT_READY GPIO 端口尚未初始化。
 * @retval FW_ERR_GPIO_CONFIG_FAILED 底层硬件配置失败。
 * @return 若端口处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用。
 */
fw_error_t core_gpio_configure_pin(core_gpio_pin_t pin,
                                   const core_gpio_pin_config_t *config);

/**
 * @brief 设置指定逻辑 GPIO 引脚的输出电平。
 * @param[in] pin 逻辑引脚标识，范围 0..(CORE_GPIO_PIN_MAX-1)。
 * @param[in] level 目标输出电平（CORE_GPIO_LEVEL_LOW 或 CORE_GPIO_LEVEL_HIGH）。
 * @retval FW_OK 电平设置成功。
 * @retval FW_ERR_GPIO_INVALID_ARG pin 越界、引脚未连接或 level 非法。
 * @retval FW_ERR_GPIO_NOT_READY GPIO 端口尚未初始化。
 * @retval FW_ERR_GPIO_IO_FAILED 底层写电平失败。
 * @return 若端口处于 FAULT，原样返回已锁存的首个故障码。
 * @note 可在任务上下文调用，不阻塞。
 */
fw_error_t core_gpio_write_pin(core_gpio_pin_t pin, core_gpio_level_t level);

/**
 * @brief 读取指定逻辑 GPIO 引脚的当前电平。
 * @param[in] pin 逻辑引脚标识，范围 0..(CORE_GPIO_PIN_MAX-1)。
 * @param[out] level 输出当前引脚电平，不得为NULL；失败时保持原值。
 * @retval FW_OK 读取成功，*level 有效。
 * @retval FW_ERR_GPIO_INVALID_ARG pin 越界、引脚未连接或 level 为NULL。
 * @retval FW_ERR_GPIO_NOT_READY GPIO 端口尚未初始化。
 * @return 若端口处于 FAULT，原样返回已锁存的首个故障码。
 * @note 可在任务或 ISR 上下文调用，不阻塞。
 */
fw_error_t core_gpio_read_pin(core_gpio_pin_t pin, core_gpio_level_t *level);

/**
 * @brief 翻转指定逻辑输出 GPIO 引脚的电平。
 * @param[in] pin 逻辑引脚标识，范围 0..(CORE_GPIO_PIN_MAX-1)。
 * @retval FW_OK 翻转成功。
 * @retval FW_ERR_GPIO_INVALID_ARG pin 越界、引脚未连接或非输出方向。
 * @retval FW_ERR_GPIO_NOT_READY GPIO 端口尚未初始化。
 * @retval FW_ERR_GPIO_IO_FAILED 底层写电平失败。
 * @return 若端口处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用；基于模块维护的输出状态翻转。
 */
fw_error_t core_gpio_toggle_pin(core_gpio_pin_t pin);

/**
 * @brief 动态切换指定逻辑 GPIO 引脚的输入/输出方向。
 * @param[in] pin 逻辑引脚标识，范围 0..(CORE_GPIO_PIN_MAX-1)。
 * @param[in] dir 目标方向枚举。
 * @retval FW_OK 方向切换成功。
 * @retval FW_ERR_GPIO_INVALID_ARG pin 越界、引脚未连接或 dir 非法。
 * @retval FW_ERR_GPIO_NOT_READY GPIO 端口尚未初始化。
 * @retval FW_ERR_GPIO_IO_FAILED 底层切换方向失败。
 * @return 若端口处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用。
 */
fw_error_t core_gpio_set_direction(core_gpio_pin_t pin, core_gpio_dir_t dir);

/**
 * @brief 为指定逻辑 GPIO 引脚注册外部中断回调并配置触发边沿（注册后默认使能该引脚中断）。
 * @param[in] pin 逻辑引脚标识，范围 0..(CORE_GPIO_PIN_MAX-1)。
 * @param[in] trig 触发边沿类型，不得为 CORE_GPIO_IRQ_NONE。
 * @param[in] cb ISR 回调函数，不得为NULL。
 * @param[in] arg 传递给 cb 的上下文指针，可为NULL。
 * @retval FW_OK 中断注册并使能成功。
 * @retval FW_ERR_GPIO_INVALID_ARG 参数越界或 cb 为NULL。
 * @retval FW_ERR_GPIO_NOT_READY GPIO 端口尚未初始化。
 * @retval FW_ERR_GPIO_ISR_FAILED 底层 ISR 服务安装或绑定失败。
 * @return 若端口处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用。
 */
fw_error_t core_gpio_register_isr(core_gpio_pin_t pin,
                                  core_gpio_irq_trig_t trig,
                                  core_gpio_isr_cb_t cb,
                                  void *arg);

/**
 * @brief 使能指定逻辑引脚已注册的外部中断。
 * @param[in] pin 逻辑引脚标识，范围 0..(CORE_GPIO_PIN_MAX-1)。
 * @retval FW_OK 使能成功。
 * @retval FW_ERR_GPIO_INVALID_ARG pin 越界或尚未注册中断回调。
 * @retval FW_ERR_GPIO_NOT_READY GPIO 端口尚未初始化。
 * @retval FW_ERR_GPIO_ISR_FAILED 底层中断使能失败。
 * @return 若端口处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用。
 */
fw_error_t core_gpio_enable_irq(core_gpio_pin_t pin);

/**
 * @brief 禁用指定逻辑引脚的外部中断。
 * @param[in] pin 逻辑引脚标识，范围 0..(CORE_GPIO_PIN_MAX-1)。
 * @retval FW_OK 禁用成功。
 * @retval FW_ERR_GPIO_INVALID_ARG pin 越界或引脚未连接。
 * @retval FW_ERR_GPIO_NOT_READY GPIO 端口尚未初始化。
 * @retval FW_ERR_GPIO_ISR_FAILED 底层中断禁用失败。
 * @return 若端口处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用。
 */
fw_error_t core_gpio_disable_irq(core_gpio_pin_t pin);

/**
 * @brief 读取 GPIO 端口当前主状态与首个失败快照（初始化失败后也可查询）。
 * @param[out] state 接收当前主状态，不得为NULL。
 * @param[out] error 接收完整失败快照，不得为NULL。
 * @retval FW_OK 快照复制成功。
 * @retval FW_ERR_GPIO_INVALID_ARG state 或 error 为NULL。
 * @note 不阻塞，不依赖串口打印。
 */
fw_error_t core_gpio_get_status(core_gpio_state_t *state, fw_error_record_t *error);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_MCU_GPIO_INC_GPIO_PORT_H */
