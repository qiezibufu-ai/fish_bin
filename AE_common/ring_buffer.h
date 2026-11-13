/**
 * @file ring_buffer.h
 * @brief 定义通用硬件无关的环形缓冲区（FIFO）接口。
 */
#ifndef AE_COMMON_RING_BUFFER_H
#define AE_COMMON_RING_BUFFER_H

#include "fw_error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 环形缓冲区控制结构体，由调用者分配存储并借用外部字节数组。 */
typedef struct
{
    uint8_t *buffer;         /**< 指向外部实际存储数据的内存数组，寿命覆盖整个缓冲区实例。 */
    uint32_t capacity_bytes; /**< 缓冲区最大容量，单位bytes，大于0。 */
    uint32_t head;           /**< 写入索引游标，仅由模块API维护。 */
    uint32_t tail;           /**< 读取索引游标，仅由模块API维护。 */
    uint32_t count_bytes;    /**< 当前已存储的数据字节数，范围0..capacity_bytes。 */
} ring_buffer_t;

/**
 * @brief 使用调用者提供的内存初始化环形缓冲区。
 * @param[out] rb 环形缓冲区实例，不得为NULL。
 * @param[in] buffer 底层字节数组，不得为NULL，寿命覆盖rb使用期。
 * @param[in] capacity_bytes 底层数组容量，单位bytes，必须大于0。
 * @retval FW_OK 初始化完成，缓冲区处于空状态。
 * @retval FW_ERR_RINGBUF_INVALID_ARG rb为NULL、buffer为NULL或capacity_bytes为0。
 * @note 不进行动态内存分配；并发访问同一rb须由调用方加锁或临界区保护。
 */
fw_error_t ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, uint32_t capacity_bytes);

/**
 * @brief 写入单个字节到环形缓冲区尾部。
 * @param[in,out] rb 已初始化的环形缓冲区实例，不得为NULL。
 * @param[in] data 待写入的字节数据。
 * @retval FW_OK 写入成功。
 * @retval FW_ERR_RINGBUF_INVALID_ARG rb或内部buffer为NULL或capacity_bytes为0。
 * @retval FW_ERR_RINGBUF_FULL 缓冲区已满，未写入数据。
 * @note 不阻塞；并发访问须由调用方串行化。
 */
fw_error_t ring_buffer_write(ring_buffer_t *rb, uint8_t data);

/**
 * @brief 从环形缓冲区头部读取并移除单个字节。
 * @param[in,out] rb 已初始化的环形缓冲区实例，不得为NULL。
 * @param[out] data 接收读取字节，不得为NULL；失败时保持原值。
 * @retval FW_OK 读取成功。
 * @retval FW_ERR_RINGBUF_INVALID_ARG rb、内部buffer或data为NULL。
 * @retval FW_ERR_RINGBUF_EMPTY 缓冲区为空，data保持原值。
 * @note 不阻塞；并发访问须由调用方串行化。
 */
fw_error_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data);

/**
 * @brief 批量写入多个字节到环形缓冲区。
 * @param[in,out] rb 已初始化的环形缓冲区实例，不得为NULL。
 * @param[in] data 待写入数据源，len_bytes大于0时不得为NULL。
 * @param[in] len_bytes 期望写入的字节数，单位bytes。
 * @param[out] written_bytes 可选输出实际写入字节数，可为NULL。
 * @retval FW_OK 全部len_bytes字节均已写入。
 * @retval FW_ERR_RINGBUF_INVALID_ARG 参数无效，written_bytes（若非NULL）置0。
 * @retval FW_ERR_RINGBUF_FULL 空间不足，仅写入前*written_bytes个字节。
 * @note 不阻塞；部分写入时通过written_bytes告知已写入的有效长度。
 */
fw_error_t ring_buffer_write_bytes(ring_buffer_t *rb, const uint8_t *data,
                                   uint32_t len_bytes, uint32_t *written_bytes);

/**
 * @brief 批量从环形缓冲区读取多个字节。
 * @param[in,out] rb 已初始化的环形缓冲区实例，不得为NULL。
 * @param[out] data 接收读取数据的目标缓冲区，len_bytes大于0时不得为NULL。
 * @param[in] len_bytes 期望读取的字节数，单位bytes。
 * @param[out] read_bytes 可选输出实际读取字节数，可为NULL。
 * @retval FW_OK 全部len_bytes字节均已读取。
 * @retval FW_ERR_RINGBUF_INVALID_ARG 参数无效，read_bytes（若非NULL）置0。
 * @retval FW_ERR_RINGBUF_EMPTY 可用字节少于len_bytes，仅前*read_bytes字节有效。
 * @note 不阻塞；部分读取时通过read_bytes告知已弹出的有效长度。
 */
fw_error_t ring_buffer_read_bytes(ring_buffer_t *rb, uint8_t *data,
                                  uint32_t len_bytes, uint32_t *read_bytes);

/**
 * @brief 判断环形缓冲区是否为空。
 * @param[in] rb 环形缓冲区实例；传NULL时返回true。
 * @return true表示为空或未初始化，false表示有可读数据。
 * @note 纯查询谓词，无副作用。
 */
bool ring_buffer_is_empty(const ring_buffer_t *rb);

/**
 * @brief 判断环形缓冲区是否已满。
 * @param[in] rb 环形缓冲区实例；传NULL时返回false。
 * @return true表示已满，false表示未满或参数为NULL。
 * @note 纯查询谓词，无副作用。
 */
bool ring_buffer_is_full(const ring_buffer_t *rb);

/**
 * @brief 获取环形缓冲区当前已存储的字节数。
 * @param[in] rb 环形缓冲区实例；传NULL时返回0。
 * @return 已存储字节数，单位bytes。
 * @note 纯查询函数，无副作用。
 */
uint32_t ring_buffer_get_count(const ring_buffer_t *rb);

/**
 * @brief 获取环形缓冲区当前剩余可写入的空闲字节数。
 * @param[in] rb 环形缓冲区实例；传NULL时返回0。
 * @return 剩余空闲字节数，单位bytes。
 * @note 纯查询函数，无副作用。
 */
uint32_t ring_buffer_get_free(const ring_buffer_t *rb);

/**
 * @brief 清空并重置环形缓冲区读写游标。
 * @param[in,out] rb 已初始化的环形缓冲区实例，不得为NULL。
 * @retval FW_OK 重置成功。
 * @retval FW_ERR_RINGBUF_INVALID_ARG rb为NULL或尚未绑定buffer。
 * @note 不擦除底层数组内容，仅将head、tail、count_bytes归零。
 */
fw_error_t ring_buffer_reset(ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif
#endif /* AE_COMMON_RING_BUFFER_H */
