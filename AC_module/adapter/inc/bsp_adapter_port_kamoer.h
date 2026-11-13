/** @file bsp_adapter_port_kamoer.h @brief 绑定Kamoer协议与UART1；不创建任务、不启动电机。 */
#ifndef AC_MODULE_ADAPTER_INC_BSP_ADAPTER_PORT_KAMOER_H
#define AC_MODULE_ADAPTER_INC_BSP_ADAPTER_PORT_KAMOER_H
#include "bsp_kamoer_driver.h"
/**
 * @brief 将驱动绑定到已由组合根初始化的UART1。
 * @param[out] motor 调用者静态实例，不得为NULL；无活动运动或并发事务。
 * @retval FW_OK 已绑定；未发送任何命令。
 * @retval FW_ERR_KAMOER_INVALID_ARG 实例为空或设备配置无效。
 * @note 不阻塞；任务上下文；UART1归应用电机任务独占。
 */
fw_error_t bsp_adapter_port_kamoer_bind(bsp_kamoer_t *motor);
#endif
