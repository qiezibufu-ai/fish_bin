/** @file motor_config.h @brief 电机设备地址、总线模式和事务预算的唯一配置源。 */
#ifndef AF_CONFIG_MOTOR_CONFIG_H
#define AF_CONFIG_MOTOR_CONFIG_H
#include <stdint.h>
/** @brief Kamoer 4460.5出厂默认地址；不使用广播地址。 */
#define MOTOR_DEVICE_ADDRESS UINT8_C(1)
/** @brief 9600bps、8N1，与S1第2位ON的出厂参数对应。 */
#define MOTOR_BAUDRATE UINT32_C(9600)
/** @brief 自动收发模块为1；当前用户已确认自动收发。 */
#define MOTOR_AUTO_DIRECTION 1
/** @brief 单次事务包含静默、发送和响应的总预算，ms。 */
#define MOTOR_TRANSACTION_TIMEOUT_MS UINT32_C(250)
#endif
