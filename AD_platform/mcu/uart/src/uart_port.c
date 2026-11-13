/**
 * @file uart_port.c
 * @brief 实现 ESP32-S3 UART / RS485 半双工平台端口、兼容适配层与可定位错误快照。
 */
#include "uart_port.h"
#include "usart_port.h"

#include "board_config.h"
#include "osal_task.h"

#include <limits.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#else
typedef int32_t esp_err_t;
#define ESP_OK 0
#define ESP_FAIL (-1)
#define ESP_ERR_TIMEOUT 0x107
#endif

/** @brief 单个逻辑 UART 通道的运行期实例状态。 */
typedef struct
{
    core_uart_config_t config;      /**< 当前通道生效配置。 */
    core_uart_state_t state;        /**< 主状态机状态。 */
    bool driver_installed;          /**< 底层驱动是否已安装。 */
    fw_error_record_t error;        /**< 首个终态失败记录。 */
    core_usart_rx_cb_t rx_cb;       /**< 兼容层接收回调。 */
    core_usart_err_cb_t err_cb;     /**< 兼容层错误回调。 */
    core_usart_tx_cb_t tx_cb;       /**< 兼容层发送回调。 */
    uint8_t *compat_rx_buf;         /**< 兼容层接收缓冲指针。 */
    uint16_t compat_rx_len;         /**< 兼容层接收缓冲长度。 */
    bool compat_rx_active;          /**< 兼容层接收会话标志。 */
} uart_channel_t;

static uart_channel_t s_uart_channels[CORE_UART_ID_MAX] = {0};

/** @brief 统一记录指定 UART 通道的首个终态失败并进入 CORE_UART_STATE_FAULT。 */
static fw_error_t uart_fail_at(core_uart_id_t id, fw_error_t code,
                               int32_t native_err, bool has_native,
                               esp_err_t cleanup_native,
                               core_uart_stage_t stage,
                               const char *file, const char *function, uint32_t line)
{
    uart_channel_t *ch = &s_uart_channels[id];
    if (ch->state == CORE_UART_STATE_FAULT && ch->error.code != FW_OK)
    {
        return ch->error.code;
    }
    fw_error_capture_at(&ch->error, code, FW_OK, (uint32_t)ch->state,
                        (uint16_t)stage, (uint32_t)id, osal_task_now_ms(),
                        file, function, line);
    ch->error.native_status = native_err;
    ch->error.native_status_valid = has_native;
    if (cleanup_native != ESP_OK)
    {
        ch->error.cleanup_code = FW_ERR_UART_DRIVER_FAILED;
    }
    ch->state = CORE_UART_STATE_FAULT;
    return ch->error.code;
}

/** @brief 在每个实际失败点捕获文件、函数和行号。 */
#define UART_FAIL(id, code, native_err, has_native, cleanup_native, stage) \
    uart_fail_at((id), (code), (native_err), (has_native), (cleanup_native), \
                 (stage), __FILE__, __func__, (uint32_t)__LINE__)

#if defined(ESP_PLATFORM)
static uart_port_t uart_hw_port(core_uart_id_t id)
{
    return (id == CORE_UART_ID_1) ?
           (uart_port_t)BOARD_UART1_HW_PORT :
           (uart_port_t)BOARD_UART2_HW_PORT;
}

static TickType_t uart_ms_to_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == 0U)
    {
        return (TickType_t)0U;
    }
    uint64_t ticks = ((uint64_t)timeout_ms * (uint64_t)configTICK_RATE_HZ + 999ULL) / 1000ULL;
    if (ticks == 0ULL)
    {
        ticks = 1ULL;
    }
    if (ticks >= (uint64_t)portMAX_DELAY)
    {
        ticks = (uint64_t)portMAX_DELAY - 1ULL;
    }
    return (TickType_t)ticks;
}

static esp_err_t hw_uart_configure_params(core_uart_id_t id,
                                          const core_uart_config_t *cfg)
{
    uart_parity_t parity = UART_PARITY_DISABLE;
    if (cfg->parity == CORE_UART_PARITY_EVEN)
    {
        parity = UART_PARITY_EVEN;
    }
    else if (cfg->parity == CORE_UART_PARITY_ODD)
    {
        parity = UART_PARITY_ODD;
    }
    uart_stop_bits_t stop_bits =
        (cfg->stop_bits == CORE_UART_STOP_BITS_2) ?
        UART_STOP_BITS_2 : UART_STOP_BITS_1;

    const uart_config_t esp_cfg = {
        .baud_rate = (int)cfg->baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = parity,
        .stop_bits = stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT
    };
    uart_port_t port = uart_hw_port(id);
    esp_err_t err = uart_param_config(port, &esp_cfg);
    if (err != ESP_OK)
    {
        return err;
    }
    int rts_pin = (cfg->rts_de_pin >= 0) ? (int)cfg->rts_de_pin : UART_PIN_NO_CHANGE;
    return uart_set_pin(port, (int)cfg->tx_pin, (int)cfg->rx_pin,
                        rts_pin, UART_PIN_NO_CHANGE);
}

static esp_err_t hw_uart_install_driver(core_uart_id_t id,
                                        const core_uart_config_t *cfg)
{
    uart_port_t port = uart_hw_port(id);
    if (uart_is_driver_installed(port))
    {
        (void)uart_driver_delete(port);
    }
    return uart_driver_install(port, (int)cfg->rx_buf_bytes,
                               (int)cfg->tx_buf_bytes, 0, NULL, 0);
}

static esp_err_t hw_uart_apply_mode(core_uart_id_t id, core_uart_mode_t mode)
{
    uart_port_t port = uart_hw_port(id);
    uart_mode_t esp_mode =
        (mode == CORE_UART_MODE_RS485_HALF_DUPLEX) ?
        UART_MODE_RS485_HALF_DUPLEX : UART_MODE_UART;
    return uart_set_mode(port, esp_mode);
}

static esp_err_t hw_uart_uninstall_driver(core_uart_id_t id)
{
    uart_port_t port = uart_hw_port(id);
    if (!uart_is_driver_installed(port))
    {
        return ESP_OK;
    }
    return uart_driver_delete(port);
}

static esp_err_t hw_uart_set_baud(core_uart_id_t id, uint32_t baudrate)
{
    return uart_set_baudrate(uart_hw_port(id), baudrate);
}

static int hw_uart_write_blocking(core_uart_id_t id, const uint8_t *data, size_t len)
{
    return uart_write_bytes(uart_hw_port(id), data, len);
}

static esp_err_t hw_uart_wait_tx_done(core_uart_id_t id, uint32_t timeout_ms)
{
    return uart_wait_tx_done(uart_hw_port(id), uart_ms_to_ticks(timeout_ms));
}

static int hw_uart_write_nonblocking(core_uart_id_t id, const uint8_t *data, size_t len)
{
    return uart_tx_chars(uart_hw_port(id), (const char *)data, (uint32_t)len);
}

static int hw_uart_read(core_uart_id_t id, uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    return uart_read_bytes(uart_hw_port(id), buf, (uint32_t)max_len,
                           uart_ms_to_ticks(timeout_ms));
}

static esp_err_t hw_uart_get_available(core_uart_id_t id, size_t *avail)
{
    return uart_get_buffered_data_len(uart_hw_port(id), avail);
}

static esp_err_t hw_uart_flush_rx(core_uart_id_t id)
{
    return uart_flush_input(uart_hw_port(id));
}
#else
/* 主机测试替身：支持注入参数、驱动、模式、卸载、收发错误，并提供内存回环验证。 */
static esp_err_t s_host_uart_param_err = ESP_OK;
static esp_err_t s_host_uart_install_err = ESP_OK;
static esp_err_t s_host_uart_mode_err = ESP_OK;
static esp_err_t s_host_uart_uninstall_err = ESP_OK;
static esp_err_t s_host_uart_wait_tx_err = ESP_OK;
static int s_host_uart_tx_ret_override = 0;
static int s_host_uart_rx_ret_override = 0;
static void (*s_host_uart_tx_hook)(core_uart_id_t, const uint8_t *, size_t) = NULL;
static uint8_t s_host_loopback_buf[CORE_UART_ID_MAX][512];
static size_t s_host_loopback_len[CORE_UART_ID_MAX] = {0};

static esp_err_t hw_uart_configure_params(core_uart_id_t id,
                                          const core_uart_config_t *cfg)
{
    (void)id;
    (void)cfg;
    return s_host_uart_param_err;
}

static esp_err_t hw_uart_install_driver(core_uart_id_t id,
                                        const core_uart_config_t *cfg)
{
    (void)id;
    (void)cfg;
    return s_host_uart_install_err;
}

static esp_err_t hw_uart_apply_mode(core_uart_id_t id, core_uart_mode_t mode)
{
    (void)id;
    (void)mode;
    return s_host_uart_mode_err;
}

static esp_err_t hw_uart_uninstall_driver(core_uart_id_t id)
{
    s_host_loopback_len[id] = 0U;
    return s_host_uart_uninstall_err;
}

static esp_err_t hw_uart_set_baud(core_uart_id_t id, uint32_t baudrate)
{
    (void)id;
    (void)baudrate;
    return s_host_uart_param_err;
}

static int hw_uart_write_blocking(core_uart_id_t id, const uint8_t *data, size_t len)
{
    if (s_host_uart_tx_ret_override != 0)
    {
        return s_host_uart_tx_ret_override;
    }
    if (s_host_uart_tx_hook != NULL)
    {
        s_host_uart_tx_hook(id, data, len);
        return (int)len;
    }
    size_t space = sizeof(s_host_loopback_buf[id]) - s_host_loopback_len[id];
    size_t copy_len = (len < space) ? len : space;
    memcpy(&s_host_loopback_buf[id][s_host_loopback_len[id]], data, copy_len);
    s_host_loopback_len[id] += copy_len;
    return (int)copy_len;
}

static esp_err_t hw_uart_wait_tx_done(core_uart_id_t id, uint32_t timeout_ms)
{
    (void)id;
    (void)timeout_ms;
    return s_host_uart_wait_tx_err;
}

static int hw_uart_write_nonblocking(core_uart_id_t id, const uint8_t *data, size_t len)
{
    return hw_uart_write_blocking(id, data, len);
}

static int hw_uart_read(core_uart_id_t id, uint8_t *buf, size_t max_len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (s_host_uart_rx_ret_override != 0)
    {
        return s_host_uart_rx_ret_override;
    }
    size_t avail = s_host_loopback_len[id];
    size_t copy_len = (max_len < avail) ? max_len : avail;
    if (copy_len > 0U)
    {
        memcpy(buf, s_host_loopback_buf[id], copy_len);
        memmove(s_host_loopback_buf[id],
                &s_host_loopback_buf[id][copy_len],
                avail - copy_len);
        s_host_loopback_len[id] -= copy_len;
    }
    return (int)copy_len;
}

static esp_err_t hw_uart_get_available(core_uart_id_t id, size_t *avail)
{
    if (s_host_uart_rx_ret_override < 0)
    {
        return ESP_FAIL;
    }
    *avail = s_host_loopback_len[id];
    return ESP_OK;
}

static esp_err_t hw_uart_flush_rx(core_uart_id_t id)
{
    if (s_host_uart_rx_ret_override < 0)
    {
        return ESP_FAIL;
    }
    s_host_loopback_len[id] = 0U;
    return ESP_OK;
}
#endif

/** @brief 校验通道配置参数合法性。 */
static bool uart_config_valid(const core_uart_config_t *cfg)
{
    if (cfg == NULL || cfg->baudrate == 0U || cfg->baudrate > 5000000U)
    {
        return false;
    }
    if (cfg->mode < CORE_UART_MODE_NORMAL ||
        cfg->mode > CORE_UART_MODE_RS485_HALF_DUPLEX)
    {
        return false;
    }
    if (cfg->parity < CORE_UART_PARITY_NONE || cfg->parity > CORE_UART_PARITY_ODD)
    {
        return false;
    }
    if (cfg->stop_bits < CORE_UART_STOP_BITS_1 ||
        cfg->stop_bits > CORE_UART_STOP_BITS_2)
    {
        return false;
    }
    if (cfg->tx_pin < 0 || cfg->tx_pin > 48 ||
        cfg->rx_pin < 0 || cfg->rx_pin > 48 ||
        cfg->tx_pin == cfg->rx_pin)
    {
        return false;
    }
    if (cfg->rts_de_pin < BOARD_GPIO_PIN_NC || cfg->rts_de_pin > 48)
    {
        return false;
    }
    if (cfg->rx_buf_bytes <= 128U || cfg->rx_buf_bytes > (uint32_t)INT_MAX ||
        cfg->tx_buf_bytes > (uint32_t)INT_MAX)
    {
        return false;
    }
    return true;
}

/** @brief 校验指定 UART 通道是否处于 READY 状态，非法枚举进入统一 FAULT 入口。 */
static fw_error_t uart_check_ready(core_uart_id_t id, core_uart_stage_t stage)
{
    uart_channel_t *ch = &s_uart_channels[id];
    switch (ch->state)
    {
        case CORE_UART_STATE_READY:
            return FW_OK;
        case CORE_UART_STATE_UNINITIALIZED:
            return FW_ERR_UART_NOT_READY;
        case CORE_UART_STATE_FAULT:
            return ch->error.code;
        default:
            return UART_FAIL(id, FW_ERR_UART_INVALID_STATE, 0, false,
                             ESP_OK, stage);
    }
}

fw_error_t core_uart_get_default_config(core_uart_id_t id,
                                        core_uart_config_t *config)
{
    if (config == NULL || id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    if (id == CORE_UART_ID_1)
    {
        *config = (core_uart_config_t){
            .baudrate = BOARD_UART1_DEFAULT_BAUDRATE,
            .mode = BOARD_UART1_RTS_PIN == BOARD_GPIO_PIN_NC ? CORE_UART_MODE_NORMAL :
                                                            CORE_UART_MODE_RS485_HALF_DUPLEX,
            .parity = CORE_UART_PARITY_NONE,
            .stop_bits = CORE_UART_STOP_BITS_1,
            .tx_pin = BOARD_UART1_TX_PIN,
            .rx_pin = BOARD_UART1_RX_PIN,
            .rts_de_pin = BOARD_UART1_RTS_PIN,
            .rx_buf_bytes = BOARD_UART1_RX_BUF_BYTES,
            .tx_buf_bytes = BOARD_UART1_TX_BUF_BYTES
        };
    }
    else
    {
        *config = (core_uart_config_t){
            .baudrate = BOARD_UART2_DEFAULT_BAUDRATE,
            .mode = BOARD_UART2_RTS_PIN == BOARD_GPIO_PIN_NC ? CORE_UART_MODE_NORMAL :
                                                            CORE_UART_MODE_RS485_HALF_DUPLEX,
            .parity = CORE_UART_PARITY_NONE,
            .stop_bits = CORE_UART_STOP_BITS_1,
            .tx_pin = BOARD_UART2_TX_PIN,
            .rx_pin = BOARD_UART2_RX_PIN,
            .rts_de_pin = BOARD_UART2_RTS_PIN,
            .rx_buf_bytes = BOARD_UART2_RX_BUF_BYTES,
            .tx_buf_bytes = BOARD_UART2_TX_BUF_BYTES
        };
    }
    return FW_OK;
}

fw_error_t core_uart_init(core_uart_id_t id, const core_uart_config_t *config)
{
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    core_uart_config_t local_cfg = {0};
    if (config == NULL)
    {
        (void)core_uart_get_default_config(id, &local_cfg);
    }
    else
    {
        local_cfg = *config;
    }

    uart_channel_t *ch = &s_uart_channels[id];
    if (ch->driver_installed)
    {
        (void)hw_uart_uninstall_driver(id);
        ch->driver_installed = false;
    }
    memset(ch, 0, sizeof(*ch));

    if (!uart_config_valid(&local_cfg))
    {
        return UART_FAIL(id, FW_ERR_UART_INVALID_ARG, 0, false,
                         ESP_OK, CORE_UART_STAGE_INIT);
    }
    esp_err_t err = hw_uart_configure_params(id, &local_cfg);
    if (err != ESP_OK)
    {
        return UART_FAIL(id, FW_ERR_UART_CONFIG_FAILED, (int32_t)err, true,
                         ESP_OK, CORE_UART_STAGE_INIT);
    }
    err = hw_uart_install_driver(id, &local_cfg);
    if (err != ESP_OK)
    {
        return UART_FAIL(id, FW_ERR_UART_DRIVER_FAILED, (int32_t)err, true,
                         ESP_OK, CORE_UART_STAGE_INIT);
    }
    ch->driver_installed = true;

    err = hw_uart_apply_mode(id, local_cfg.mode);
    if (err != ESP_OK)
    {
        esp_err_t cleanup_err = hw_uart_uninstall_driver(id);
        ch->driver_installed = false;
        return UART_FAIL(id, FW_ERR_UART_CONFIG_FAILED, (int32_t)err, true,
                         cleanup_err, CORE_UART_STAGE_INIT);
    }

    ch->config = local_cfg;
    ch->state = CORE_UART_STATE_READY;
    return FW_OK;
}

fw_error_t core_uart_deinit(core_uart_id_t id)
{
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    uart_channel_t *ch = &s_uart_channels[id];
    if (ch->driver_installed)
    {
        esp_err_t err = hw_uart_uninstall_driver(id);
        if (err != ESP_OK)
        {
            return UART_FAIL(id, FW_ERR_UART_DRIVER_FAILED, (int32_t)err, true,
                             ESP_OK, CORE_UART_STAGE_DEINIT);
        }
        ch->driver_installed = false;
    }
    ch->compat_rx_active = false;
    ch->compat_rx_buf = NULL;
    ch->compat_rx_len = 0U;
    ch->state = CORE_UART_STATE_UNINITIALIZED;
    return FW_OK;
}

fw_error_t core_uart_set_baudrate(core_uart_id_t id, uint32_t baudrate)
{
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX ||
        baudrate == 0U || baudrate > 5000000U)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    fw_error_t ready_err = uart_check_ready(id, CORE_UART_STAGE_CONFIG);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    esp_err_t err = hw_uart_set_baud(id, baudrate);
    if (err != ESP_OK)
    {
        return UART_FAIL(id, FW_ERR_UART_CONFIG_FAILED, (int32_t)err, true,
                         ESP_OK, CORE_UART_STAGE_CONFIG);
    }
    s_uart_channels[id].config.baudrate = baudrate;
    return FW_OK;
}

fw_error_t core_uart_tx_poll(core_uart_id_t id,
                             const uint8_t *data,
                             size_t len_bytes,
                             uint32_t timeout_ms)
{
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX ||
        data == NULL || len_bytes == 0U ||
        timeout_ms == 0U || timeout_ms == UINT32_MAX)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    fw_error_t ready_err = uart_check_ready(id, CORE_UART_STAGE_TX);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    int written = hw_uart_write_blocking(id, data, len_bytes);
    if (written < 0 || (size_t)written != len_bytes)
    {
        return UART_FAIL(id, FW_ERR_UART_TX_FAILED, (int32_t)written, true,
                         ESP_OK, CORE_UART_STAGE_TX);
    }
    esp_err_t wait_err = hw_uart_wait_tx_done(id, timeout_ms);
    if (wait_err == ESP_ERR_TIMEOUT)
    {
        return FW_ERR_UART_TIMEOUT;
    }
    if (wait_err != ESP_OK)
    {
        return UART_FAIL(id, FW_ERR_UART_TX_FAILED, (int32_t)wait_err, true,
                         ESP_OK, CORE_UART_STAGE_TX);
    }
    return FW_OK;
}

fw_error_t core_uart_tx_frame(core_uart_id_t id, const uint8_t *data,
                              size_t len_bytes, uint32_t timeout_ms)
{
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX || data == NULL ||
        len_bytes == 0U || len_bytes > 64U || timeout_ms == 0U || timeout_ms == UINT32_MAX)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    fw_error_t ready = uart_check_ready(id, CORE_UART_STAGE_TX);
    if (ready != FW_OK)
    {
        return ready;
    }
    if (s_uart_channels[id].config.tx_buf_bytes != 0U)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    int written = hw_uart_write_nonblocking(id, data, len_bytes);
    if (written < 0 || (size_t)written != len_bytes)
    {
        return UART_FAIL(id, FW_ERR_UART_TX_FAILED, (int32_t)written, true,
                         ESP_OK, CORE_UART_STAGE_TX);
    }
    esp_err_t error = hw_uart_wait_tx_done(id, timeout_ms);
    if (error == ESP_ERR_TIMEOUT)
    {
        return FW_ERR_UART_TIMEOUT;
    }
    if (error != ESP_OK)
    {
        return UART_FAIL(id, FW_ERR_UART_TX_FAILED, (int32_t)error, true,
                         ESP_OK, CORE_UART_STAGE_TX);
    }
    return FW_OK;
}

fw_error_t core_uart_tx_async(core_uart_id_t id,
                              const uint8_t *data,
                              size_t len_bytes,
                              size_t *queued_bytes)
{
    if (queued_bytes != NULL)
    {
        *queued_bytes = 0U;
    }
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX ||
        data == NULL || len_bytes == 0U)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    fw_error_t ready_err = uart_check_ready(id, CORE_UART_STAGE_TX);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    int written = hw_uart_write_nonblocking(id, data, len_bytes);
    if (written < 0)
    {
        return UART_FAIL(id, FW_ERR_UART_TX_FAILED, (int32_t)written, true,
                         ESP_OK, CORE_UART_STAGE_TX);
    }
    if (queued_bytes != NULL)
    {
        *queued_bytes = (size_t)written;
    }
    if ((size_t)written < len_bytes)
    {
        return FW_ERR_UART_BUSY;
    }
    return FW_OK;
}

fw_error_t core_uart_rx_read(core_uart_id_t id,
                             uint8_t *buf,
                             size_t max_len_bytes,
                             size_t *out_read_bytes,
                             uint32_t timeout_ms)
{
    if (out_read_bytes != NULL)
    {
        *out_read_bytes = 0U;
    }
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX ||
        buf == NULL || max_len_bytes == 0U || out_read_bytes == NULL ||
        timeout_ms == UINT32_MAX)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    fw_error_t ready_err = uart_check_ready(id, CORE_UART_STAGE_RX);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    int read_len = hw_uart_read(id, buf, max_len_bytes, timeout_ms);
    if (read_len < 0)
    {
        return UART_FAIL(id, FW_ERR_UART_RX_FAILED, (int32_t)read_len, true,
                         ESP_OK, CORE_UART_STAGE_RX);
    }
    *out_read_bytes = (size_t)read_len;
    return FW_OK;
}

fw_error_t core_uart_rx_available(core_uart_id_t id, size_t *available_bytes)
{
    if (available_bytes != NULL)
    {
        *available_bytes = 0U;
    }
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX || available_bytes == NULL)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    fw_error_t ready_err = uart_check_ready(id, CORE_UART_STAGE_RX);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    size_t local_avail = 0U;
    esp_err_t err = hw_uart_get_available(id, &local_avail);
    if (err != ESP_OK)
    {
        return UART_FAIL(id, FW_ERR_UART_RX_FAILED, (int32_t)err, true,
                         ESP_OK, CORE_UART_STAGE_RX);
    }
    *available_bytes = local_avail;
    return FW_OK;
}

fw_error_t core_uart_rx_flush(core_uart_id_t id)
{
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    fw_error_t ready_err = uart_check_ready(id, CORE_UART_STAGE_RX);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    esp_err_t err = hw_uart_flush_rx(id);
    if (err != ESP_OK)
    {
        return UART_FAIL(id, FW_ERR_UART_RX_FAILED, (int32_t)err, true,
                         ESP_OK, CORE_UART_STAGE_RX);
    }
    return FW_OK;
}

fw_error_t core_uart_get_status(core_uart_id_t id,
                                core_uart_state_t *state,
                                fw_error_record_t *error)
{
    if (id < CORE_UART_ID_1 || id >= CORE_UART_ID_MAX ||
        state == NULL || error == NULL)
    {
        return FW_ERR_UART_INVALID_ARG;
    }
    *state = s_uart_channels[id].state;
    *error = s_uart_channels[id].error;
    return FW_OK;
}

/* ==================== 26CTCCv1.2 兼容薄适配层实现 ========================== */

platform_err_t core_usart_reg_isr(en_core_usart_t id,
                                  core_usart_rx_cb_t rx_cb,
                                  core_usart_err_cb_t err_cb)
{
    if ((int)id < 0 || id >= EN_CORE_USART_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }
    s_uart_channels[id].rx_cb = rx_cb;
    s_uart_channels[id].err_cb = err_cb;
    return PLATFORM_ERR_OK;
}

platform_err_t core_usart_reg_tx_isr(en_core_usart_t id,
                                     core_usart_tx_cb_t tx_cb)
{
    if ((int)id < 0 || id >= EN_CORE_USART_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }
    s_uart_channels[id].tx_cb = tx_cb;
    return PLATFORM_ERR_OK;
}

platform_err_t core_usart_rx_start(en_core_usart_t id,
                                   uint8_t *buf,
                                   uint16_t len)
{
    if ((int)id < 0 || id >= EN_CORE_USART_NUM || buf == NULL || len == 0U)
    {
        return PLATFORM_ERR_PARAM;
    }
    fw_error_t ready_err = uart_check_ready((core_uart_id_t)id, CORE_UART_STAGE_RX);
    if (ready_err != FW_OK)
    {
        return fw_error_to_platform_err(ready_err);
    }
    s_uart_channels[id].compat_rx_buf = buf;
    s_uart_channels[id].compat_rx_len = len;
    s_uart_channels[id].compat_rx_active = true;
    return PLATFORM_ERR_OK;
}

platform_err_t core_usart_rx_stop(en_core_usart_t id)
{
    if ((int)id < 0 || id >= EN_CORE_USART_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }
    s_uart_channels[id].compat_rx_active = false;
    s_uart_channels[id].compat_rx_buf = NULL;
    s_uart_channels[id].compat_rx_len = 0U;
    return PLATFORM_ERR_OK;
}

platform_err_t core_usart_tx_dma(en_core_usart_t id,
                                 const uint8_t *buf,
                                 uint16_t len)
{
    if ((int)id < 0 || id >= EN_CORE_USART_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }
    size_t queued = 0U;
    fw_error_t err = core_uart_tx_async((core_uart_id_t)id, buf, (size_t)len, &queued);
    if (err == FW_OK && s_uart_channels[id].tx_cb != NULL)
    {
        (void)s_uart_channels[id].tx_cb();
    }
    return fw_error_to_platform_err(err);
}

platform_err_t core_usart_tx_stop(en_core_usart_t id)
{
    if ((int)id < 0 || id >= EN_CORE_USART_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }
    return PLATFORM_ERR_OK;
}

platform_err_t core_usart_tx_poll(en_core_usart_t id,
                                  const uint8_t *buf,
                                  uint16_t len,
                                  uint32_t timeout_ms)
{
    if ((int)id < 0 || id >= EN_CORE_USART_NUM)
    {
        return PLATFORM_ERR_PARAM;
    }
    fw_error_t err = core_uart_tx_poll((core_uart_id_t)id, buf,
                                       (size_t)len, timeout_ms);
    return fw_error_to_platform_err(err);
}
