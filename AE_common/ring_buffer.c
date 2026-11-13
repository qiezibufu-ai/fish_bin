/**
 * @file ring_buffer.c
 * @brief 实现硬件无关的环形缓冲区（FIFO）。
 */
#include "ring_buffer.h"

fw_error_t ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, uint32_t capacity_bytes)
{
    if (rb == NULL || buffer == NULL || capacity_bytes == 0U)
    {
        return FW_ERR_RINGBUF_INVALID_ARG;
    }
    rb->buffer = buffer;
    rb->capacity_bytes = capacity_bytes;
    rb->head = 0U;
    rb->tail = 0U;
    rb->count_bytes = 0U;
    return FW_OK;
}

fw_error_t ring_buffer_write(ring_buffer_t *rb, uint8_t data)
{
    if (rb == NULL || rb->buffer == NULL || rb->capacity_bytes == 0U)
    {
        return FW_ERR_RINGBUF_INVALID_ARG;
    }
    if (rb->count_bytes >= rb->capacity_bytes)
    {
        return FW_ERR_RINGBUF_FULL;
    }
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1U) % rb->capacity_bytes;
    rb->count_bytes++;
    return FW_OK;
}

fw_error_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data)
{
    if (rb == NULL || rb->buffer == NULL || rb->capacity_bytes == 0U || data == NULL)
    {
        return FW_ERR_RINGBUF_INVALID_ARG;
    }
    if (rb->count_bytes == 0U)
    {
        return FW_ERR_RINGBUF_EMPTY;
    }
    uint8_t value = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1U) % rb->capacity_bytes;
    rb->count_bytes--;
    *data = value;
    return FW_OK;
}

fw_error_t ring_buffer_write_bytes(ring_buffer_t *rb, const uint8_t *data,
                                   uint32_t len_bytes, uint32_t *written_bytes)
{
    if (written_bytes != NULL)
    {
        *written_bytes = 0U;
    }
    if (rb == NULL || rb->buffer == NULL || rb->capacity_bytes == 0U ||
        (len_bytes > 0U && data == NULL))
    {
        return FW_ERR_RINGBUF_INVALID_ARG;
    }
    uint32_t count = 0U;
    for (uint32_t i = 0U; i < len_bytes; ++i)
    {
        if (ring_buffer_write(rb, data[i]) != FW_OK)
        {
            break;
        }
        count++;
    }
    if (written_bytes != NULL)
    {
        *written_bytes = count;
    }
    return (count == len_bytes) ? FW_OK : FW_ERR_RINGBUF_FULL;
}

fw_error_t ring_buffer_read_bytes(ring_buffer_t *rb, uint8_t *data,
                                  uint32_t len_bytes, uint32_t *read_bytes)
{
    if (read_bytes != NULL)
    {
        *read_bytes = 0U;
    }
    if (rb == NULL || rb->buffer == NULL || rb->capacity_bytes == 0U ||
        (len_bytes > 0U && data == NULL))
    {
        return FW_ERR_RINGBUF_INVALID_ARG;
    }
    uint32_t count = 0U;
    for (uint32_t i = 0U; i < len_bytes; ++i)
    {
        if (ring_buffer_read(rb, &data[i]) != FW_OK)
        {
            break;
        }
        count++;
    }
    if (read_bytes != NULL)
    {
        *read_bytes = count;
    }
    return (count == len_bytes) ? FW_OK : FW_ERR_RINGBUF_EMPTY;
}

bool ring_buffer_is_empty(const ring_buffer_t *rb)
{
    if (rb == NULL || rb->buffer == NULL || rb->capacity_bytes == 0U)
    {
        return true;
    }
    return rb->count_bytes == 0U;
}

bool ring_buffer_is_full(const ring_buffer_t *rb)
{
    if (rb == NULL || rb->buffer == NULL || rb->capacity_bytes == 0U)
    {
        return false;
    }
    return rb->count_bytes >= rb->capacity_bytes;
}

uint32_t ring_buffer_get_count(const ring_buffer_t *rb)
{
    if (rb == NULL || rb->buffer == NULL || rb->capacity_bytes == 0U)
    {
        return 0U;
    }
    return rb->count_bytes;
}

uint32_t ring_buffer_get_free(const ring_buffer_t *rb)
{
    if (rb == NULL || rb->buffer == NULL || rb->capacity_bytes == 0U ||
        rb->count_bytes >= rb->capacity_bytes)
    {
        return 0U;
    }
    return rb->capacity_bytes - rb->count_bytes;
}

fw_error_t ring_buffer_reset(ring_buffer_t *rb)
{
    if (rb == NULL || rb->buffer == NULL || rb->capacity_bytes == 0U)
    {
        return FW_ERR_RINGBUF_INVALID_ARG;
    }
    rb->head = 0U;
    rb->tail = 0U;
    rb->count_bytes = 0U;
    return FW_OK;
}
