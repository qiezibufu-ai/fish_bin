/** @file drop_count_config.h @brief 液滴计数输入配置。 */
#ifndef AF_CONFIG_DROP_COUNT_CONFIG_H
#define AF_CONFIG_DROP_COUNT_CONFIG_H

#include <stdint.h>

/** @brief PCNT短毛刺滤波阈值，单位ns；不用于抑制液滴拉丝。 */
#define DROP_COUNT_GLITCH_NS UINT32_C(1000)

#endif
