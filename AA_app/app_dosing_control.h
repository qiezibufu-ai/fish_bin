/** @file app_dosing_control.h @brief 人工标定与串口滴定应用入口。 */
#ifndef AA_APP_APP_DOSING_CONTROL_H
#define AA_APP_APP_DOSING_CONTROL_H
#include "service_dosing.h"

/**
 * @brief 初始化应用状态为未标定；不发任何电机命令。
 * @param[in,out] dosing 已初始化的滴定服务，寿命覆盖应用，非空。
 * @note 启动阶段调用；之后控制任务独占，面板与ADC须已初始化。
 */
void app_dosing_control_init(service_dosing_t *dosing);

/**
 * @brief 轮询按键、接单、采样、推进滴定并更新灯色。
 * @note 仅所有者任务每10ms调用；不等待串口输入或灯发送；
 * 滴定步进的RS485事务预算沿用service_dosing_step。
 */
void app_dosing_control_step(void);
/**
 * @brief 复制最近应用故障记录，新标定清零。
 * @param[out] error 非空，接收诊断。
 * @retval FW_OK 已复制；code为FW_OK表示无锁存错误。
 * @retval FW_ERR_PORT_INVALID_ARG 参数为空。
 * @note 不等待，仅控制任务调用或在该任务停止后查询。
 */
fw_error_t app_dosing_control_get_error(fw_error_record_t *error);
#endif
