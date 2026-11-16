/** @file panel_port.h @brief GPIO0按键、GPIO48像素与命令串口接收平台端口。 */
#ifndef AD_PLATFORM_MCU_PANEL_INC_PANEL_PORT_H
#define AD_PLATFORM_MCU_PANEL_INC_PANEL_PORT_H
#include "fw_error.h"
#include <stddef.h>

/** @brief 命令接收通道，各自保持独立的字节流。 */
typedef enum
{
    CORE_PANEL_RX_UART0, /**< UART0，经外部USB转串口。 */
    CORE_PANEL_RX_USB,   /**< ESP32-S3原生USB Serial/JTAG。 */
    CORE_PANEL_RX_COUNT  /**< 接收通道数量。 */
} core_panel_rx_t;

/**
 * @brief 配置BOOT上拉输入、RMT及UART0、原生USB的非阻塞读取。
 * @param[out] native 非空；原生SDK错误或errno；成功清零。
 * @retval FW_OK 端口已就绪。
 * @retval FW_ERR_PORT_IO 初始化失败。
 * @retval FW_ERR_PORT_INVALID_ARG 参数为空。
 * @note 启动任务中调用一次；失败后由调用方deinit清理；串行独占。
 */
fw_error_t core_panel_init(int32_t *native);
/**
 * @brief 停止并释放面板端口。
 * @retval FW_OK 已释放或尚未初始化。
 * @retval FW_ERR_PORT_IO 清理失败，可显式重试。
 * @note 维护时调用，禁止与所有者并发；RMT等待上限20ms加调度误差。
 */
fw_error_t core_panel_deinit(void);
/**
 * @brief 读取BOOT引脚的即时电平。
 * @return true表示低电平按下，false表示释放。
 * @note 初始化后任务上下文调用；不延时，不消抖。
 */
bool core_panel_boot_pressed(void);
/**
 * @brief 获取指定通道当前可用字符，不等待新数据。
 * @param[in] source UART0或原生USB；未编译的USB通道返回0字节。
 * @param[out] data 非空，至少capacity字节；失败时内容无效。
 * @param[in] capacity 最大读取字节数，1..32。
 * @param[out] count 非空，实际字节数；无数据时为0。
 * @retval FW_OK 成功或无可读数据。
 * @retval FW_ERR_PORT_IO 读取失败。
 * @retval FW_ERR_PORT_INVALID_ARG 参数无效。
 * @note 初始化后由控制任务独占读取；UART0与日志共用，不重设波特率。
 */
fw_error_t core_panel_read(core_panel_rx_t source, char *data, size_t capacity, size_t *count);
/**
 * @brief 提交一个像素颜色；上一次仍发送时立即返回BUSY。
 * @param[in] red 红分量，0..255。
 * @param[in] green 绿分量，0..255。
 * @param[in] blue 蓝分量，0..255。
 * @retval FW_OK 已提交，RMT异步发送。
 * @retval FW_ERR_PORT_BUSY 上一帧仍在发送。
 * @retval FW_ERR_PORT_IO 底层错误。
 * @note 初始化后由同一任务调用；不等待RMT发送完成。
 */
fw_error_t core_panel_rgb(uint8_t red, uint8_t green, uint8_t blue);
#endif
