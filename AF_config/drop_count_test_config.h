/** @file drop_count_test_config.h @brief 液滴计数台架与采集配置。 */
#ifndef AF_CONFIG_DROP_COUNT_TEST_CONFIG_H
#define AF_CONFIG_DROP_COUNT_TEST_CONFIG_H

#include <stdint.h>

/** @brief 1启用只读计数台架，优先于电机上电测试；0恢复原电机模式。 */
#ifndef APP_DROP_COUNT_TEST_ENABLE
#define APP_DROP_COUNT_TEST_ENABLE 1
#endif
/** @brief 台架读取硬件累计值的周期，单位ms；无变化时不重复输出。 */
#define APP_DROP_COUNT_REPORT_PERIOD_MS UINT32_C(200)
/** @brief PCNT短毛刺滤波阈值，单位ns；不用于抑制液滴拉丝。 */
#define DROP_COUNT_GLITCH_NS UINT32_C(1000)

#endif
