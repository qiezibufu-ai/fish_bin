#include "panel_port.h"

#include "board_config.h"

#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "sdkconfig.h"
#if CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG || CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#include "driver/usb_serial_jtag.h"
#define PANEL_USB_RX_ENABLED 1
#endif

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int s_console = -1;
#if PANEL_USB_RX_ENABLED
static bool s_usb_owned;
#endif
static rmt_channel_handle_t s_channel;
static rmt_encoder_handle_t s_encoder;
static bool s_enabled;
static bool s_pending;
/* RMT借用直到发送结束，不使用栈缓冲。 */
static rmt_symbol_word_t s_symbols[25];

fw_error_t core_panel_init(int32_t *native)
{
    if (native == NULL)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    const gpio_config_t button = {
        .pin_bit_mask = UINT64_C(1) << BOARD_GPIO_BOOT_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    const rmt_tx_channel_config_t channel = {
        .gpio_num = BOARD_GPIO_STATUS_LED_PIN, .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 20000000U, .mem_block_symbols = 64U, .trans_queue_depth = 1U
    };
    const rmt_copy_encoder_config_t encoder = {};
    esp_err_t err = gpio_config(&button);
    if (err == ESP_OK)
    {
        err = rmt_new_tx_channel(&channel, &s_channel);
    }
    if (err == ESP_OK)
    {
        err = rmt_new_copy_encoder(&encoder, &s_encoder);
    }
    if (err == ESP_OK)
    {
        err = rmt_enable(s_channel);
        s_enabled = err == ESP_OK;
    }
    *native = err;
    if (err != ESP_OK)
    {
        return FW_ERR_PORT_IO;
    }
    s_console = open("/dev/uart/0", O_RDONLY | O_NONBLOCK);
    if (s_console < 0)
    {
        *native = errno;
        return FW_ERR_PORT_IO;
    }
#if PANEL_USB_RX_ENABLED
    /* 辅助控制台只复制日志；由USB驱动的RX缓冲直接接收命令。 */
    if (!usb_serial_jtag_is_driver_installed())
    {
        usb_serial_jtag_driver_config_t usb = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        err = usb_serial_jtag_driver_install(&usb);
        *native = err;
        if (err != ESP_OK)
        {
            return FW_ERR_PORT_IO;
        }
        s_usb_owned = true;
    }
#endif
    return FW_OK;
}

fw_error_t core_panel_deinit(void)
{
#if PANEL_USB_RX_ENABLED
    if (s_usb_owned)
    {
        if (usb_serial_jtag_driver_uninstall() != ESP_OK)
        {
            return FW_ERR_PORT_IO;
        }
        s_usb_owned = false;
    }
#endif
    if (s_console >= 0)
    {
        int fd = s_console;
        s_console = -1;
        if (close(fd) != 0)
        {
            return FW_ERR_PORT_IO;
        }
    }
    if (s_enabled)
    {
        if (rmt_tx_wait_all_done(s_channel, 20) != ESP_OK || rmt_disable(s_channel) != ESP_OK)
        {
            return FW_ERR_PORT_IO;
        }
        s_pending = false;
        s_enabled = false;
    }
    if (s_encoder != NULL)
    {
        if (rmt_del_encoder(s_encoder) != ESP_OK)
        {
            return FW_ERR_PORT_IO;
        }
        s_encoder = NULL;
    }
    if (s_channel != NULL)
    {
        if (rmt_del_channel(s_channel) != ESP_OK)
        {
            return FW_ERR_PORT_IO;
        }
        s_channel = NULL;
    }
    return FW_OK;
}

bool core_panel_boot_pressed(void)
{
    return gpio_get_level(BOARD_GPIO_BOOT_PIN) == 0;
}

fw_error_t core_panel_read(core_panel_rx_t source, char *data, size_t capacity, size_t *count)
{
    if ((source != CORE_PANEL_RX_UART0 && source != CORE_PANEL_RX_USB) ||
        data == NULL || count == NULL || capacity == 0U || capacity > 32U)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    *count = 0U;
    if (source == CORE_PANEL_RX_USB)
    {
#if PANEL_USB_RX_ENABLED
        if (!usb_serial_jtag_is_driver_installed())
        {
            return FW_ERR_PORT_IO;
        }
        int n = usb_serial_jtag_read_bytes(data, (uint32_t)capacity, 0U);
        if (n < 0)
        {
            return FW_ERR_PORT_IO;
        }
        *count = (size_t)n;
#endif
        return FW_OK;
    }
    ssize_t n = read(s_console, data, capacity);
    if (n >= 0)
    {
        *count = (size_t)n;
        return FW_OK;
    }
    return errno == EAGAIN || errno == EWOULDBLOCK ? FW_OK : FW_ERR_PORT_IO;
}

fw_error_t core_panel_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_pending)
    {
        esp_err_t done = rmt_tx_wait_all_done(s_channel, 0);
        if (done != ESP_OK)
        {
            return done == ESP_ERR_TIMEOUT ? FW_ERR_PORT_BUSY : FW_ERR_PORT_IO;
        }
        s_pending = false;
    }

    const uint8_t grb[] = {green, red, blue};
    for (size_t bit = 0; bit < 24U; ++bit)
    {
        bool one = (grb[bit / 8U] & (0x80U >> (bit % 8U))) != 0U;
        s_symbols[bit] = (rmt_symbol_word_t){
            .level0 = 1, .duration0 = one ? 18 : 7, .level1 = 0, .duration1 = one ? 7 : 18
        };
    }
    /* 300us低电平锁存。 */
    s_symbols[24] = (rmt_symbol_word_t){.duration0 = 3000, .duration1 = 3000};
    const rmt_transmit_config_t tx = {.flags.queue_nonblocking = 1};
    esp_err_t err = rmt_transmit(s_channel, s_encoder, s_symbols, sizeof(s_symbols), &tx);
    s_pending = err == ESP_OK;
    return err == ESP_OK ? FW_OK : FW_ERR_PORT_IO;
}
