/**
 * @file osal_status.h
 * @brief 定义与具体 RTOS 无关的 OSAL 状态码及向 fw_error_t 的转换接口。
 */
#ifndef AD_PLATFORM_OS_INC_OSAL_STATUS_H
#define AD_PLATFORM_OS_INC_OSAL_STATUS_H

#include "fw_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief OSAL 公共返回状态码枚举。 */
typedef enum
{
    OSAL_OK = 0,            /**< 操作成功。 */
    OSAL_ERR_PARAM,         /**< 输入参数非法（空指针、长度为0或越界）。 */
    OSAL_ERR_TIMEOUT,       /**< 等待超时，未获取到资源或数据。 */
    OSAL_ERR_NO_MEMORY,     /**< 内核对象或动态内存不足。 */
    OSAL_ERR_BUSY,          /**< 队列已满或资源当前忙。 */
    OSAL_ERR_NOT_SUPPORTED, /**< 当前执行上下文（如 ISR）或配置不支持该操作。 */
    OSAL_ERR_STATE,         /**< 资源状态非法（如非持有者释放互斥锁、计数溢出）。 */
    OSAL_ERR_FAIL           /**< 底层 RTOS 内核操作失败。 */
} osal_status_t;

/**
 * @brief 将 OSAL 状态码显式转换为工程统一错误码 fw_error_t。
 * @param[in] status OSAL 接口返回的状态码。
 * @retval FW_OK 输入为 OSAL_OK。
 * @retval FW_ERR_OSAL_INVALID_ARG 输入为 OSAL_ERR_PARAM。
 * @retval FW_ERR_OSAL_TIMEOUT 输入为 OSAL_ERR_TIMEOUT。
 * @retval FW_ERR_OSAL_NO_MEMORY 输入为 OSAL_ERR_NO_MEMORY。
 * @retval FW_ERR_OSAL_BUSY 输入为 OSAL_ERR_BUSY。
 * @retval FW_ERR_OSAL_NOT_SUPPORTED 输入为 OSAL_ERR_NOT_SUPPORTED。
 * @retval FW_ERR_OSAL_INVALID_STATE 输入为 OSAL_ERR_STATE。
 * @retval FW_ERR_OSAL_FAIL 输入为 OSAL_ERR_FAIL 或未定义值。
 * @note 纯转换函数，无副作用，可在任意上下文调用。
 */
static inline fw_error_t osal_status_to_fw_error(osal_status_t status)
{
    switch (status)
    {
        case OSAL_OK:
            return FW_OK;
        case OSAL_ERR_PARAM:
            return FW_ERR_OSAL_INVALID_ARG;
        case OSAL_ERR_TIMEOUT:
            return FW_ERR_OSAL_TIMEOUT;
        case OSAL_ERR_NO_MEMORY:
            return FW_ERR_OSAL_NO_MEMORY;
        case OSAL_ERR_BUSY:
            return FW_ERR_OSAL_BUSY;
        case OSAL_ERR_NOT_SUPPORTED:
            return FW_ERR_OSAL_NOT_SUPPORTED;
        case OSAL_ERR_STATE:
            return FW_ERR_OSAL_INVALID_STATE;
        case OSAL_ERR_FAIL:
        default:
            return FW_ERR_OSAL_FAIL;
    }
}

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_OS_INC_OSAL_STATUS_H */
