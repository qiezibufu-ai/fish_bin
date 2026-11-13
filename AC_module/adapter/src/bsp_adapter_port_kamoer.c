/** @file bsp_adapter_port_kamoer.c @brief 自动收发TTL/RS485串口事务适配，单次总预算。 */
#include "bsp_adapter_port_kamoer.h"
#include "motor_config.h"
#include "osal_task.h"
#include "uart_port.h"
#include <string.h>

static uint32_t motor_now_ms(void *context)
{
    (void)context;
    return osal_task_now_ms();
}

static uint32_t remaining_ms(uint32_t started, uint32_t budget)
{
    uint32_t elapsed = osal_task_now_ms() - started;
    return elapsed < budget ? budget - elapsed : 0U;
}

static fw_error_t capture_port_error(fw_error_t code, fw_error_record_t *origin)
{
    core_uart_state_t state;
    (void)core_uart_get_status(CORE_UART_ID_1, &state, origin);
    return code;
}

/** @brief 仅UART1所有者调用；帧最长13字节，TX无软件环形缓冲，禁止其他生产者写入。 */
static fw_error_t motor_exchange(void *context, const uint8_t *tx, size_t tx_bytes,
                                 uint8_t *rx, size_t capacity, size_t *rx_bytes,
                                 uint32_t timeout_ms, fw_error_record_t *origin)
{
    (void)context;
    *rx_bytes = 0U;
    memset(origin, 0, sizeof(*origin));
    uint32_t started = osal_task_now_ms();
    /* 保守按11bit字符计算3.5字符间隔，9600bps向上取整为5ms。 */
    uint32_t silence_ms = (38500U + MOTOR_BAUDRATE - 1U) / MOTOR_BAUDRATE;
    osal_status_t os_error = osal_task_delay_ms(silence_ms);
    if (os_error != OSAL_OK)
    {
        return osal_status_to_fw_error(os_error);
    }
    fw_error_t error = core_uart_rx_flush(CORE_UART_ID_1);
    if (error != FW_OK)
    {
        return capture_port_error(error, origin);
    }
    uint32_t remaining = remaining_ms(started, timeout_ms);
    if (remaining == 0U)
    {
        return FW_ERR_PORT_TIMEOUT;
    }
    error = core_uart_tx_frame(CORE_UART_ID_1, tx, tx_bytes, remaining);
    if (error != FW_OK)
    {
        return capture_port_error(error, origin);
    }
    size_t used = 0U;
    size_t expected = 3U;
    while (remaining_ms(started, timeout_ms) != 0U)
    {
        size_t read_bytes = 0U;
        error = core_uart_rx_read(CORE_UART_ID_1, &rx[used], expected - used,
                                  &read_bytes, 0U);
        if (error != FW_OK)
        {
            return capture_port_error(error, origin);
        }
        used += read_bytes;
        if (used == expected)
        {
            if (expected == 3U)
            {
                if ((rx[1] & 0x80U) != 0U)
                {
                    expected = 5U;
                }
                else if (rx[1] == 0x03U || rx[1] == 0x04U)
                {
                    expected = (size_t)rx[2] + 5U;
                }
                else if (rx[1] == 0x05U || rx[1] == 0x10U)
                {
                    expected = 8U;
                }
                else
                {
                    *rx_bytes = used;
                    return FW_OK; /* 交给协议层报告格式错误。 */
                }
                if (expected > capacity)
                {
                    *rx_bytes = used;
                    return FW_OK;
                }
            }
            else
            {
                *rx_bytes = used;
                return FW_OK;
            }
        }
        if (read_bytes == 0U)
        {
            os_error = osal_task_delay_ms(1U);
            if (os_error != OSAL_OK)
            {
                return osal_status_to_fw_error(os_error);
            }
        }
    }
    return FW_ERR_PORT_TIMEOUT;
}

fw_error_t bsp_adapter_port_kamoer_bind(bsp_kamoer_t *motor)
{
    const bsp_kamoer_ops_t ops = {
        .exchange = motor_exchange,
        .now_ms = motor_now_ms,
        .context = NULL
    };
    return bsp_kamoer_init(motor, &ops, MOTOR_DEVICE_ADDRESS, MOTOR_TRANSACTION_TIMEOUT_MS);
}
