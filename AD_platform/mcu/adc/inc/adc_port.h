/** @file adc_port.h @brief 板级空管模拟输入的ADC单次采集端口。 */
#ifndef AD_PLATFORM_MCU_ADC_INC_ADC_PORT_H
#define AD_PLATFORM_MCU_ADC_INC_ADC_PORT_H

#include "fw_error.h"

/** @brief 同一次ADC转换的原始值和校准电压。 */
typedef struct
{
    int raw;            /**< 12位原始值，范围0..4095。 */
    int voltage_mv;     /**< GPIO上的电压，单位mV；仅在voltage_valid为true时有效。 */
    bool voltage_valid; /**< 芯片校准可用且换算成功。 */
} core_adc_sample_t;

/**
 * @brief 初始化板级模拟输入及可用的芯片校准。
 * @param[out] error 非空；成功清零，失败返回原生状态、阶段和清理情况。
 * @retval FW_OK 可采集；板上无校准数据时仍支持原始值。
 * @retval FW_ERR_PORT_INVALID_ARG error为空。
 * @retval FW_ERR_ADC_INIT 配置或资源申请失败。
 * @note 启动任务调用，SDK会申请资源；全部端口API由一个所有者串行调用。
 */
fw_error_t core_adc_init(fw_error_record_t *error);

/**
 * @brief 执行一次ADC转换，使用同一原始样本计算电压。
 * @param[out] sample 非空，读取失败时保持原值。
 * @param[out] error 非空，成功清零；转换失败时返回原生状态。
 * @retval FW_OK 有原始样本，使用voltage_valid确认是否有电压。
 * @retval FW_ERR_PORT_INVALID_ARG 参数为空。
 * @retval FW_ERR_PORT_NOT_READY 未完成初始化。
 * @retval FW_ERR_ADC_READ 单次ADC采集或校准换算失败。
 * @note 任务上下文；只进行一次SDK转换，不等待下一次采样时刻，不重试。
 */
fw_error_t core_adc_read(core_adc_sample_t *sample, fw_error_record_t *error);

/**
 * @brief 释放校准及ADC资源，重复释放无副作用。
 * @param[out] error 非空；成功清零，失败保存原生状态。
 * @retval FW_OK 已释放。
 * @retval FW_ERR_PORT_INVALID_ARG error为空。
 * @retval FW_ERR_ADC_RELEASE SDK释放失败，可在停止采样后重试。
 * @note 维护任务调用，不能与采样并发。
 */
fw_error_t core_adc_deinit(fw_error_record_t *error);

#endif
