/** @file dosing_test_config.h @brief 清水台架单次滴定请求。 */
#ifndef AF_CONFIG_DOSING_TEST_CONFIG_H
#define AF_CONFIG_DOSING_TEST_CONFIG_H
#include <stdint.h>
/** @brief 1每次上电执行一轮清水滴定；0保留原台架模式。 */
#ifndef APP_DOSING_TEST_ENABLE
#define APP_DOSING_TEST_ENABLE 1
#endif
/** @brief 应用所有者调度周期，单位ms。 */
#define APP_DOSING_TEST_PERIOD_MS UINT32_C(20)
/** @brief 清水试验的停泵阈值，单位滴。 */
#define APP_DOSING_TEST_TARGET_DROPS UINT32_C(7)
/** @brief 清水试验泵速20rpm，单位0.01rpm。 */
#define APP_DOSING_TEST_SPEED_CENTI_RPM UINT32_C(2000)
/** @brief 0顺时针，1逆时针；按现场出液方向设置。 */
#define APP_DOSING_TEST_REVERSE 0
/** @brief 首滴及滴间超时，单位ms。 */
#define APP_DOSING_TEST_NO_DROP_MS UINT32_C(5000)
/** @brief 从发START前开始计算的运行上限，单位ms。 */
#define APP_DOSING_TEST_MAX_RUN_MS UINT32_C(20000)
/** @brief STOP应答后至少连续无滴时间，单位ms。 */
#define APP_DOSING_TEST_TAIL_QUIET_MS UINT32_C(500)
/** @brief STOP应答后尾滴观察上限，单位ms。 */
#define APP_DOSING_TEST_TAIL_MAX_MS UINT32_C(3000)
#endif
