/** @file dosing_control_config.h @brief BOOT标定、滴定交互与状态灯配置。 */
#ifndef AF_CONFIG_DOSING_CONTROL_CONFIG_H
#define AF_CONFIG_DOSING_CONTROL_CONFIG_H
#include <stdint.h>

/** @brief 正向滴定及回收速度，0.01rpm；3000表示30rpm。 */
#define DOSING_CONTROL_PUMP_SPEED_CENTI_RPM UINT32_C(3000)
/** @brief 正向滴定方向：0顺时针，1逆时针；回收自动取反。 */
#define DOSING_CONTROL_REVERSE 0
/** @brief 正向STOP应答后至少连续无新滴的等待时间，ms。 */
#define DOSING_CONTROL_TAIL_QUIET_MS UINT32_C(500)
/** @brief 正向STOP应答后尾滴观察上限，ms。 */
#define DOSING_CONTROL_TAIL_MAX_MS UINT32_C(3000)

/** @brief 采集并平均的样本数；必须为正，且不大于1000。 */
#ifndef BUBBLE_CALIBRATION_SAMPLES
#define BUBBLE_CALIBRATION_SAMPLES 40U
#endif
/** @brief 同一标定窗口最大值减最小值的上限，mV。 */
#ifndef BUBBLE_CALIBRATION_SPREAD_MV
#define BUBBLE_CALIBRATION_SPREAD_MV 150U
#endif
/** @brief 实时电压高于空管基线超过该值时判有液，mV。 */
#ifndef BUBBLE_LIQUID_DELTA_MV
#define BUBBLE_LIQUID_DELTA_MV 800U
#endif
/** @brief 正向滴定运行上限；从发START前起算，ms。 */
#ifndef DOSING_CONTROL_TIMEOUT_MS
#define DOSING_CONTROL_TIMEOUT_MS 30000U
#endif
/** @brief 标定及正向运行时液体诊断采样间隔，ms。 */
#define DOSING_CONTROL_SAMPLE_MS 20U
/** @brief BOOT按键消抖时间，ms。 */
#define DOSING_CONTROL_BUTTON_MS 30U
/** @brief 应用循环目标周期，ms；低于OS时钟节拍时以节拍为准。 */
#define DOSING_CONTROL_PERIOD_MS 10U
/** @brief WS2812单通道亮度，0..255。 */
#define DOSING_CONTROL_LED_LEVEL 24U
#endif
