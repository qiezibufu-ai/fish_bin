/**
 * @file usart_port.h
 * @brief 提供兼容 26CTCCv1.2 旧工程 core_usart_* 接口的薄适配声明。
 * @details 新模块应直接包含 uart_port.h 并使用返回 fw_error_t 的 core_uart_* 接口；
 * 本头文件仅用于保持旧上层模块调用签名与返回值语义兼容。
 */
#ifndef AD_PLATFORM_MCU_UART_INC_USART_PORT_H
#define AD_PLATFORM_MCU_UART_INC_USART_PORT_H

#include "platform_error.h"
#include "uart_port.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 兼容旧工程的逻辑串口枚举。 */
typedef enum
{
    EN_CORE_USART_BUS1 = 0,               /**< 映射至 CORE_UART_ID_1。 */
    EN_CORE_USART_BUS2 = 1,               /**< 映射至 CORE_UART_ID_2。 */
    EN_CORE_USART_IMU = EN_CORE_USART_BUS1, /**< 兼容旧工程名称别名。 */
    EN_CORE_USART_NUM = CORE_UART_ID_MAX  /**< 逻辑串口总数。 */
} en_core_usart_t;

/**
 * @brief 兼容旧工程的接收完成回调类型。
 * @param[in] size 本次接收到的数据字节数，单位bytes。
 * @return PLATFORM_ERR_OK 或具体平台错误码。
 */
typedef platform_err_t (*core_usart_rx_cb_t)(uint16_t size);

/**
 * @brief 兼容旧工程的串口错误恢复回调类型。
 * @return PLATFORM_ERR_OK 或具体平台错误码。
 */
typedef platform_err_t (*core_usart_err_cb_t)(void);

/**
 * @brief 兼容旧工程的异步发送完成回调类型。
 * @return PLATFORM_ERR_OK 或具体平台错误码。
 */
typedef platform_err_t (*core_usart_tx_cb_t)(void);

/**
 * @brief 注册兼容层串口接收与错误回调。
 * @param[in] id 逻辑串口枚举。
 * @param[in] rx_cb 接收回调，可为NULL。
 * @param[in] err_cb 错误回调，可为NULL。
 * @retval PLATFORM_ERR_OK 注册成功。
 * @retval PLATFORM_ERR_PARAM id 越界。
 * @note 仅任务上下文调用。
 */
platform_err_t core_usart_reg_isr(en_core_usart_t id,
                                  core_usart_rx_cb_t rx_cb,
                                  core_usart_err_cb_t err_cb);

/**
 * @brief 注册兼容层串口发送完成回调。
 * @param[in] id 逻辑串口枚举。
 * @param[in] tx_cb 发送完成回调，可为NULL。
 * @retval PLATFORM_ERR_OK 注册成功。
 * @retval PLATFORM_ERR_PARAM id 越界。
 * @note 仅任务上下文调用。
 */
platform_err_t core_usart_reg_tx_isr(en_core_usart_t id,
                                     core_usart_tx_cb_t tx_cb);

/**
 * @brief 兼容层启动接收会话并绑定接收缓冲区。
 * @param[in] id 逻辑串口枚举。
 * @param[out] buf 接收数据缓冲区，不得为NULL。
 * @param[in] len 接收缓冲区容量，单位bytes，必须大于0。
 * @retval PLATFORM_ERR_OK 接收会话已就绪。
 * @retval PLATFORM_ERR_PARAM 参数无效。
 * @retval PLATFORM_ERR_NOT_INITIALIZED 通道尚未初始化。
 * @note 仅任务上下文调用。
 */
platform_err_t core_usart_rx_start(en_core_usart_t id,
                                   uint8_t *buf,
                                   uint16_t len);

/**
 * @brief 兼容层停止当前接收会话。
 * @param[in] id 逻辑串口枚举。
 * @retval PLATFORM_ERR_OK 已停止。
 * @retval PLATFORM_ERR_PARAM id 越界。
 * @note 仅任务上下文调用。
 */
platform_err_t core_usart_rx_stop(en_core_usart_t id);

/**
 * @brief 兼容层非阻塞异步发送数据（内部委托 core_uart_tx_async）。
 * @param[in] id 逻辑串口枚举。
 * @param[in] buf 待发送数据缓冲区，不得为NULL。
 * @param[in] len 待发送长度，单位bytes，必须大于0。
 * @retval PLATFORM_ERR_OK 已全部写入驱动发送缓冲。
 * @return 其他错误由 fw_error_to_platform_err() 显式映射。
 * @note 仅任务上下文调用。
 */
platform_err_t core_usart_tx_dma(en_core_usart_t id,
                                 const uint8_t *buf,
                                 uint16_t len);

/**
 * @brief 兼容层终止发送会话。
 * @param[in] id 逻辑串口枚举。
 * @retval PLATFORM_ERR_OK 操作完成。
 * @retval PLATFORM_ERR_PARAM id 越界。
 * @note 仅任务上下文调用。
 */
platform_err_t core_usart_tx_stop(en_core_usart_t id);

/**
 * @brief 兼容层轮询有界阻塞发送数据（内部委托 core_uart_tx_poll）。
 * @param[in] id 逻辑串口枚举。
 * @param[in] buf 待发送数据缓冲区，不得为NULL。
 * @param[in] len 待发送长度，单位bytes，必须大于0。
 * @param[in] timeout_ms 最大超时等待，单位ms，必须大于0。
 * @retval PLATFORM_ERR_OK 发送完成。
 * @return 其他错误由 fw_error_to_platform_err() 显式映射。
 * @note 仅任务上下文调用。
 */
platform_err_t core_usart_tx_poll(en_core_usart_t id,
                                  const uint8_t *buf,
                                  uint16_t len,
                                  uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_MCU_UART_INC_USART_PORT_H */
