/**
 * @file platform_error.h
 * @brief 提供与旧工程兼容的 platform_err_t 定义及向 fw_error_t 的显式映射。
 */
#ifndef AE_COMMON_PLATFORM_ERROR_H
#define AE_COMMON_PLATFORM_ERROR_H

#include "fw_error.h"
#include "platform_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 兼容 26CTCCv1.2 旧接口的平台层错误枚举，保留原数值ABI。 */
typedef enum
{
    PLATFORM_ERR_OK              = 0,          /**< 操作成功。 */
    PLATFORM_ERR_GENERAL         = 1,          /**< 通用平台错误。 */
    PLATFORM_ERR_TIMEOUT         = 2,          /**< 事务或等待超时。 */
    PLATFORM_ERR_PARAM           = 3,          /**< 输入参数非法。 */
    PLATFORM_ERR_NO_MEMORY       = 4,          /**< 内存分配不足。 */
    PLATFORM_ERR_NO_RESOURCE     = 5,          /**< 硬件或系统资源不足。 */
    PLATFORM_ERR_NOT_SUPPORTED   = 6,          /**< 当前平台或上下文不支持。 */
    PLATFORM_ERR_NOT_INITIALIZED = 7,          /**< 模块尚未初始化。 */
    PLATFORM_ERR_ALREADY_INIT    = 8,          /**< 模块已初始化。 */
    PLATFORM_ERR_BUSY            = 9,          /**< 外设或总线忙。 */
    PLATFORM_ERR_FAIL            = 10,         /**< 底层传输或执行失败。 */
    PLATFORM_ERR_RESERVED        = 0x7FFFFFFF  /**< 枚举位宽保留值。 */
} platform_err_t;

/** @brief 判断兼容错误码是否为失败。 */
#define PLATFORM_IS_ERR(err) ((err) != PLATFORM_ERR_OK)
/** @brief 判断兼容错误码是否为成功。 */
#define PLATFORM_IS_OK(err)  ((err) == PLATFORM_ERR_OK)

/**
 * @brief 将兼容的 platform_err_t 显式映射为统一 fw_error_t。
 * @param[in] err 旧平台错误码。
 * @retval FW_OK 输入为 PLATFORM_ERR_OK。
 * @return 对应的 FW_ERR_PORT_* 符号错误码。
 * @note 纯转换函数，无副作用，可在任意上下文调用。
 */
static inline fw_error_t platform_err_to_fw_error(platform_err_t err)
{
    switch (err)
    {
        case PLATFORM_ERR_OK:
            return FW_OK;
        case PLATFORM_ERR_TIMEOUT:
            return FW_ERR_PORT_TIMEOUT;
        case PLATFORM_ERR_PARAM:
            return FW_ERR_PORT_INVALID_ARG;
        case PLATFORM_ERR_NO_MEMORY:
        case PLATFORM_ERR_NO_RESOURCE:
            return FW_ERR_PORT_NO_RESOURCE;
        case PLATFORM_ERR_NOT_SUPPORTED:
            return FW_ERR_PORT_NOT_SUPPORTED;
        case PLATFORM_ERR_NOT_INITIALIZED:
            return FW_ERR_PORT_NOT_READY;
        case PLATFORM_ERR_ALREADY_INIT:
        case PLATFORM_ERR_BUSY:
            return FW_ERR_PORT_BUSY;
        case PLATFORM_ERR_GENERAL:
        case PLATFORM_ERR_FAIL:
        default:
            return FW_ERR_PORT_IO;
    }
}

/**
 * @brief 将统一 fw_error_t 映射回兼容的 platform_err_t。
 * @param[in] err 统一错误码。
 * @retval PLATFORM_ERR_OK 输入为 FW_OK。
 * @return 对应的 platform_err_t 枚举值。
 * @note 仅供历史兼容薄适配层使用，新模块应直接使用 fw_error_t。
 */
static inline platform_err_t fw_error_to_platform_err(fw_error_t err)
{
    switch (err)
    {
        case FW_OK:
            return PLATFORM_ERR_OK;
        case FW_ERR_PORT_TIMEOUT:
        case FW_ERR_OSAL_TIMEOUT:
        case FW_ERR_UART_TIMEOUT:
            return PLATFORM_ERR_TIMEOUT;
        case FW_ERR_PORT_INVALID_ARG:
        case FW_ERR_OSAL_INVALID_ARG:
        case FW_ERR_GPIO_INVALID_ARG:
        case FW_ERR_UART_INVALID_ARG:
        case FW_ERR_RINGBUF_INVALID_ARG:
        case FW_ERR_APP_INVALID_ARG:
            return PLATFORM_ERR_PARAM;
        case FW_ERR_OSAL_NO_MEMORY:
            return PLATFORM_ERR_NO_MEMORY;
        case FW_ERR_PORT_NO_RESOURCE:
            return PLATFORM_ERR_NO_RESOURCE;
        case FW_ERR_PORT_NOT_SUPPORTED:
        case FW_ERR_OSAL_NOT_SUPPORTED:
            return PLATFORM_ERR_NOT_SUPPORTED;
        case FW_ERR_PORT_NOT_READY:
        case FW_ERR_GPIO_NOT_READY:
        case FW_ERR_UART_NOT_READY:
        case FW_ERR_APP_NOT_READY:
            return PLATFORM_ERR_NOT_INITIALIZED;
        case FW_ERR_APP_ALREADY_INIT:
            return PLATFORM_ERR_ALREADY_INIT;
        case FW_ERR_PORT_BUSY:
        case FW_ERR_OSAL_BUSY:
        case FW_ERR_UART_BUSY:
            return PLATFORM_ERR_BUSY;
        default:
            return PLATFORM_ERR_FAIL;
    }
}

#ifdef __cplusplus
}
#endif
#endif /* AE_COMMON_PLATFORM_ERROR_H */
