/** @file service_bubble.h @brief 空管电压标定与相对电压判定。 */
#ifndef AB_SERVICE_BUBBLE_INC_SERVICE_BUBBLE_H
#define AB_SERVICE_BUBBLE_INC_SERVICE_BUBBLE_H
#include "fw_error.h"

/** @brief 调用方静态存储；标定状态由API维护。 */
typedef struct
{
    uint32_t samples;        /**< 本轮已接收样本数。 */
    uint32_t sum_mv;         /**< 本轮样本电压和，mV。 */
    uint32_t min_mv;         /**< 窗口内最低电压，mV。 */
    uint32_t max_mv;         /**< 窗口内最高电压，mV。 */
    uint32_t baseline_mv;    /**< 空管基准，mV；仅valid时有效。 */
    bool valid;              /**< 基准已标定。 */
    bool complete;           /**< 窗口完成，成功或失败。 */
} service_bubble_t;

/**
 * @brief 开始新标定，丢弃上一轮基准。
 * @param[out] self 非空实例。
 * @note 不等待；由同一任务调用。
 */
void service_bubble_reset(service_bubble_t *self);

/**
 * @brief 累积一个校准后的电压；达到配置采样数时验证极差并取平均。
 * @param[in,out] self 非空实例；每轮先reset。
 * @param[in] mv 本次引脚电压，0..3600mV。
 * @retval FW_OK 采样接受；complete为true时基准已生效。
 * @retval FW_ERR_BUBBLE_UNSTABLE 窗口极差超限，标定失败并结束。
 * @retval FW_ERR_PORT_INVALID_ARG 参数无效。
 * @retval FW_ERR_PORT_NOT_READY 本轮已结束，须reset才能重新采样。
 * @note 不等待；由同一任务调用。
 */
fw_error_t service_bubble_add(service_bubble_t *self, uint32_t mv);

/**
 * @brief 用实时电压对比空管基准；增量严格大于阈值才判有液。
 * @param[in] self 已标定的非空实例。
 * @param[in] mv 本次引脚电压，0..3600mV。
 * @param[out] present 非空；失败时保持原值。
 * @retval FW_OK 判定有效。
 * @retval FW_ERR_PORT_INVALID_ARG 参数无效。
 * @retval FW_ERR_PORT_NOT_READY 尚无有效基准。
 * @note 不等待，不访问硬件；由同一任务调用。
 */
fw_error_t service_bubble_liquid(const service_bubble_t *self, uint32_t mv, bool *present);
#endif
