/**
 * @file board_config.h
 * @brief 定义 ESP32-S3 板级外设引脚映射、串口参数与任务资源预算。
 * @details 作为板级配置的唯一权威源，仅使用标准整数常量描述硬件映射，
 * 不向纯驱动或算法层传递 ESP-IDF HAL 头文件。
 */
#ifndef AF_CONFIG_BOARD_CONFIG_H
#define AF_CONFIG_BOARD_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== GPIO 板级引脚映射 (ESP32-S3) ========================= */
/** @brief 未连接或不使用硬件引脚时的哨兵值。 */
#define BOARD_GPIO_PIN_NC (-1)

/** @brief 由面板RMT独占的板载WS2812输出引脚。 */
#define BOARD_GPIO_STATUS_LED_PIN 48
/** @brief ESP32-S3板载BOOT按键，低电平有效。 */
#define BOARD_GPIO_BOOT_PIN 0
/** @brief RS485 总线1收发方向控制 DE/RE 引脚（自动收发模块不使用；由UART独占RTS时也不在GPIO层配置）。 */
#define BOARD_GPIO_RS485_BUS1_DE_PIN BOARD_GPIO_PIN_NC
/** @brief RS485 总线2收发方向控制 DE/RE 引脚（默认 GPIO8，不使用时设为 BOARD_GPIO_PIN_NC）。 */
#define BOARD_GPIO_RS485_BUS2_DE_PIN BOARD_GPIO_PIN_NC
/** @brief 外部传感器脉冲/触发输入引脚（默认 GPIO4）。 */
#define BOARD_GPIO_SENSOR_TRIG_PIN 4

/** @brief QLT1616计数脉冲候选引脚，复用原传感器输入；接入前需完成NPN/PNP电平适配。 */
#define BOARD_GPIO_DROP_COUNT_PIN BOARD_GPIO_SENSOR_TRIG_PIN
/** @brief 空管传感器模拟输出GPIO5，对应ESP32-S3 ADC1_CH4。 */
#define BOARD_GPIO_BUBBLE_PIN 5
/** @brief 手动方向模块可使用GPIO18作为UART RTS；GPIO层不重复配置。 */
#define BOARD_MOTOR_MANUAL_RTS_PIN 18

/* ==================== UART / RS485 板级通道配置 ============================ */
/** @brief 逻辑串口1绑定的 ESP32-S3 硬件 UART 控制器编号（UART_NUM_1 = 1）。 */
#define BOARD_UART1_HW_PORT 1
/** @brief 逻辑串口1 TX 引脚编号（GPIO17）。 */
#define BOARD_UART1_TX_PIN 17
/** @brief 逻辑串口1 RX 引脚编号（GPIO16）。 */
#define BOARD_UART1_RX_PIN 16
/** @brief 逻辑串口1 RTS/DE 引脚编号（复用 BOARD_GPIO_RS485_BUS1_DE_PIN）。 */
#define BOARD_UART1_RTS_PIN BOARD_GPIO_RS485_BUS1_DE_PIN
/** @brief 逻辑串口1 默认通信波特率，单位 bps。 */
#define BOARD_UART1_DEFAULT_BAUDRATE UINT32_C(9600)
/** @brief 逻辑串口1 接收驱动环形缓冲区大小，单位 bytes。 */
#define BOARD_UART1_RX_BUF_BYTES UINT32_C(512)
/** @brief 逻辑串口1 发送驱动环形缓冲区大小，单位 bytes（0 表示同步写 FIFO）。 */
#define BOARD_UART1_TX_BUF_BYTES UINT32_C(256)

/** @brief 逻辑串口2绑定的 ESP32-S3 硬件 UART 控制器编号（UART_NUM_2 = 2）。 */
#define BOARD_UART2_HW_PORT 2
/** @brief 逻辑串口2 TX 引脚编号（GPIO19）。 */
#define BOARD_UART2_TX_PIN 19
/** @brief 逻辑串口2 RX 引脚编号（GPIO20）。 */
#define BOARD_UART2_RX_PIN 20
/** @brief 逻辑串口2 RTS/DE 引脚编号（复用 BOARD_GPIO_RS485_BUS2_DE_PIN）。 */
#define BOARD_UART2_RTS_PIN BOARD_GPIO_RS485_BUS2_DE_PIN
/** @brief 逻辑串口2 默认通信波特率，单位 bps。 */
#define BOARD_UART2_DEFAULT_BAUDRATE UINT32_C(115200)
/** @brief 逻辑串口2 接收驱动环形缓冲区大小，单位 bytes。 */
#define BOARD_UART2_RX_BUF_BYTES UINT32_C(512)
/** @brief 逻辑串口2 发送驱动环形缓冲区大小，单位 bytes。 */
#define BOARD_UART2_TX_BUF_BYTES UINT32_C(256)

/** @brief 串口轮询发送默认超时预算，单位 ms。 */
#define BOARD_UART_DEFAULT_TX_TIMEOUT_MS UINT32_C(100)
/** @brief 串口读取默认超时预算，单位 ms。 */
#define BOARD_UART_DEFAULT_RX_TIMEOUT_MS UINT32_C(50)

/* ==================== 应用组合根任务配置 =================================== */
/** @brief 默认引导监控任务栈大小，单位 bytes。 */
#define BOARD_TASK_BOOTSTRAP_STACK_BYTES UINT32_C(4096)
/** @brief 默认引导监控任务优先级。 */
#define BOARD_TASK_BOOTSTRAP_PRIORITY UINT32_C(5)

#ifdef __cplusplus
}
#endif
#endif /* AF_CONFIG_BOARD_CONFIG_H */
