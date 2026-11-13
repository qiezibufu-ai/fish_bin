/**
 * @file osal_queue.h
 * @brief 定义 OSAL 消息队列接口（发送/接收/查看/计数自动适配任务与 ISR 上下文）。
 */
#ifndef AD_PLATFORM_OS_INC_OSAL_QUEUE_H
#define AD_PLATFORM_OS_INC_OSAL_QUEUE_H

#include "osal_status.h"
#include "osal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建固定深度、固定元素大小的消息队列（按值复制元素）。
 * @param[in] queue_depth 队列最大元素个数，必须大于0。
 * @param[in] data_size 单个元素字节大小，单位bytes，必须大于0。
 * @param[out] queue_handle 输出队列句柄，不得为NULL；失败时置为NULL。
 * @retval OSAL_OK 创建成功。
 * @retval OSAL_ERR_PARAM queue_handle 为NULL，或 queue_depth/data_size 为0或溢出。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @retval OSAL_ERR_NO_MEMORY 内核堆空间不足。
 * @note 仅任务上下文调用。
 */
osal_status_t osal_queue_create(size_t queue_depth,
                                size_t data_size,
                                osal_queue_handle_t *queue_handle);

/**
 * @brief 删除消息队列并释放内核存储。
 * @param[in] queue_handle 待删除的队列句柄，不得为NULL。
 * @retval OSAL_OK 删除成功。
 * @retval OSAL_ERR_PARAM queue_handle 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用。
 */
osal_status_t osal_queue_delete(osal_queue_handle_t queue_handle);

/**
 * @brief 向队列尾部发送一项数据（按创建时的 data_size 复制内存）。
 * @param[in] queue_handle 目标队列句柄，不得为NULL。
 * @param[in] data 待发送元素指针，不得为NULL，返回后调用者可立即复用缓冲区。
 * @param[in] timeout_ms 任务上下文最大等待时间，单位ms；ISR 中忽略并使用非阻塞发送。
 * @retval OSAL_OK 元素已成功入队。
 * @retval OSAL_ERR_PARAM queue_handle 或 data 为NULL。
 * @retval OSAL_ERR_BUSY 超时后队列仍满，未写入数据。
 * @note 可在任务或 ISR 上下文调用。
 */
osal_status_t osal_queue_send(osal_queue_handle_t queue_handle,
                              const void *data,
                              osal_tick_type_t timeout_ms);

/**
 * @brief 从队列头部接收并移除一项数据。
 * @param[in] queue_handle 目标队列句柄，不得为NULL。
 * @param[out] data 接收元素缓冲区，不得为NULL，容量至少为创建时的 data_size；失败时不修改。
 * @param[in] timeout_ms 任务上下文最大等待时间，单位ms；ISR 中忽略并使用非阻塞接收。
 * @retval OSAL_OK 已成功复制并移除队首元素。
 * @retval OSAL_ERR_PARAM queue_handle 或 data 为NULL。
 * @retval OSAL_ERR_TIMEOUT 超时或队列为空，无新数据。
 * @note 可在任务或 ISR 上下文调用。
 */
osal_status_t osal_queue_receive(osal_queue_handle_t queue_handle,
                                 void *data,
                                 osal_tick_type_t timeout_ms);

/**
 * @brief 查看队列头部数据但不从队列移除。
 * @param[in] queue_handle 目标队列句柄，不得为NULL。
 * @param[out] data 接收元素缓冲区，不得为NULL，容量至少为 data_size。
 * @param[in] timeout_ms 任务上下文最大等待时间，单位ms；ISR 中忽略并使用非阻塞查看。
 * @retval OSAL_OK 已成功复制队首元素。
 * @retval OSAL_ERR_PARAM queue_handle 或 data 为NULL。
 * @retval OSAL_ERR_TIMEOUT 超时或队列为空。
 * @note 可在任务或 ISR 上下文调用。
 */
osal_status_t osal_queue_peek(osal_queue_handle_t queue_handle,
                              void *data,
                              osal_tick_type_t timeout_ms);

/**
 * @brief 查询当前队列中已存储的元素个数。
 * @param[in] queue_handle 目标队列句柄，不得为NULL。
 * @param[out] count 输出当前元素个数，不得为NULL；失败时置0。
 * @retval OSAL_OK 查询成功。
 * @retval OSAL_ERR_PARAM queue_handle 或 count 为NULL。
 * @retval OSAL_ERR_FAIL 底层计数超出 uint32_t 范围。
 * @note 可在任务或 ISR 上下文调用，不阻塞。
 */
osal_status_t osal_queue_count(osal_queue_handle_t queue_handle,
                               uint32_t *count);

/**
 * @brief 清空消息队列中的所有待处理元素。
 * @param[in] queue_handle 目标队列句柄，不得为NULL。
 * @retval OSAL_OK 队列已清空重置。
 * @retval OSAL_ERR_PARAM queue_handle 为NULL。
 * @retval OSAL_ERR_NOT_SUPPORTED 在 ISR 上下文中调用。
 * @note 仅任务上下文调用。
 */
osal_status_t osal_queue_reset(osal_queue_handle_t queue_handle);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_OS_INC_OSAL_QUEUE_H */
