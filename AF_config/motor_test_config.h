/** @file motor_test_config.h @brief 应用电机台架测试参数；不作为药量标定参数。 */
#ifndef AF_CONFIG_MOTOR_TEST_CONFIG_H
#define AF_CONFIG_MOTOR_TEST_CONFIG_H
#include <stdint.h>
/** @brief 1开启一次上电台架测试；默认0，仅绑定驱动，无电机总线输出。 */
#ifndef APP_MOTOR_TEST_ENABLE
#define APP_MOTOR_TEST_ENABLE 0
#endif
/** @brief 默认顺时针；0=CW，1=CCW，实际出液方向需台架确认。 */
#define APP_MOTOR_TEST_DIRECTION 0
/** @brief 测试速度10rpm，编码单位0.01rpm；范围100..50000。 */
#define APP_MOTOR_TEST_SPEED_CENTI_RPM UINT32_C(10000)
/** @brief 起始速度10rpm，不大于测试速度。 */
#define APP_MOTOR_TEST_START_SPEED_CENTI_RPM UINT32_C(10000)
/** @brief 启动ACK后运行3000ms，然后发STOP；不包含减速及管路余液时间。 */
#define APP_MOTOR_TEST_DURATION_MS UINT32_C(3000)
#endif
