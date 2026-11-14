/** @file pcnt_port.h @brief 板级液滴输入的PCNT单通道采集端口。 */
#ifndef AD_PLATFORM_MCU_PCNT_INC_PCNT_PORT_H
#define AD_PLATFORM_MCU_PCNT_INC_PCNT_PORT_H

#include "fw_error.h"

/**
 * @brief 连接已配置的板级液滴输入，清零并开始累计下降沿。
 * @retval FW_OK 已开始采集，重复调用时保持当前计数。
 * @retval FW_ERR_PCNT_INIT 初始化失败，原生状态保存在首错记录中。
 * @return 若故障已锁存，则返回首错；须显式反初始化后才能重试。
 * @note 启动任务调用；GPIO输入须先初始化。初始化会申请SDK资源，不用于实时热路径。
 * 模块仅有一个实例，初始化、读取和释放均须由上层串行化。
 */
fw_error_t core_pcnt_init(void);

/**
 * @brief 获取累计的有效下降沿数；不清零、不等待脉冲。
 * @param[out] count 累计脉冲数，不得为NULL，失败保持原值。
 * @retval FW_OK 已读取硬件及溢出补偿值；零也是有效读数。
 * @retval FW_ERR_PORT_INVALID_ARG 输出参数为空。
 * @retval FW_ERR_PORT_NOT_READY 尚未初始化。
 * @retval FW_ERR_PCNT_READ SDK读取失败或达到台架计数上限。
 * @return 已锁存故障时返回首错。
 * @note 任务上下文；与初始化/释放串行；不等待IO或锁住应用资源。
 */
fw_error_t core_pcnt_read(uint32_t *count);

/**
 * @brief 停止采集并释放PCNT资源；保留首错到下一次初始化。
 * @retval FW_OK 已释放或未分配资源。
 * @retval FW_ERR_PCNT_RELEASE 释放失败，保留未释放的句柄以供显式重试。
 * @note 任务上下文；调用前停止采样任务，释放后方可反初始化GPIO。
 */
fw_error_t core_pcnt_deinit(void);

/**
 * @brief 复制最近一次首错记录；可在清理后读取。
 * @param[out] error 错误记录，不得为NULL；无故障时code为FW_OK。
 * @retval FW_OK 已复制。
 * @retval FW_ERR_PORT_INVALID_ARG 输出参数为空。
 * @note 不阻塞；与采样及生命周期接口串行调用。
 */
fw_error_t core_pcnt_get_error(fw_error_record_t *error);

#endif
