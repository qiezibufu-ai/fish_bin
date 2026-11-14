#include "pcnt_port.h"

#include "board_config.h"
#include "drop_count_test_config.h"
#include "osal_task.h"

#include "driver/pulse_cnt.h"
#include <limits.h>
#include <stddef.h>

typedef enum
{
    PCNT_PORT_OFF = 0,
    PCNT_PORT_RUNNING,
    PCNT_PORT_FAULT
} pcnt_port_state_t;

typedef enum
{
    PCNT_PORT_STAGE_UNIT = 1,
    PCNT_PORT_STAGE_CHANNEL,
    PCNT_PORT_STAGE_EDGE,
    PCNT_PORT_STAGE_FILTER,
    PCNT_PORT_STAGE_LIMIT,
    PCNT_PORT_STAGE_ENABLE,
    PCNT_PORT_STAGE_CLEAR,
    PCNT_PORT_STAGE_START,
    PCNT_PORT_STAGE_READ,
    PCNT_PORT_STAGE_RELEASE,
    PCNT_PORT_STAGE_VALIDATE
} pcnt_port_stage_t;

enum { PCNT_COUNT_LIMIT = 32767 };
static pcnt_unit_handle_t s_unit;
static pcnt_channel_handle_t s_channel;
static bool s_enabled;
static bool s_started;
static pcnt_port_state_t s_state;
static fw_error_record_t s_error;

/* 按SDK生命周期释放；失败时保留剩余资源，以便显式重试。 */
static esp_err_t pcnt_release(void)
{
    esp_err_t err;
    if (s_started)
    {
        err = pcnt_unit_stop(s_unit);
        if (err != ESP_OK)
        {
            return err;
        }
        s_started = false;
    }
    if (s_enabled)
    {
        err = pcnt_unit_disable(s_unit);
        if (err != ESP_OK)
        {
            return err;
        }
        s_enabled = false;
    }
    if (s_channel != NULL)
    {
        err = pcnt_del_channel(s_channel);
        if (err != ESP_OK)
        {
            return err;
        }
        s_channel = NULL;
    }
    if (s_unit != NULL)
    {
        err = pcnt_del_unit(s_unit);
        if (err != ESP_OK)
        {
            return err;
        }
        s_unit = NULL;
    }
    return ESP_OK;
}

static fw_error_t pcnt_fail_at(fw_error_t code, pcnt_port_stage_t stage, esp_err_t native,
                               const char *file, const char *function, uint32_t line)
{
    if (s_state != PCNT_PORT_FAULT)
    {
        fw_error_capture_at(&s_error, code, FW_OK, (uint32_t)s_state, (uint16_t)stage, 0U,
                            osal_task_now_ms(), file, function, line);
        s_error.native_status = (int32_t)native;
        s_error.native_status_valid = native != ESP_OK;
        s_state = PCNT_PORT_FAULT;
    }
    return s_error.code;
}

#define PCNT_FAIL(code, stage, native) \
    pcnt_fail_at((code), (stage), (native), __FILE__, __func__, (uint32_t)__LINE__)

static fw_error_t pcnt_cleanup_after_error(fw_error_t code)
{
    if (pcnt_release() != ESP_OK)
    {
        s_error.cleanup_code = FW_ERR_PCNT_RELEASE;
    }
    return code;
}

fw_error_t core_pcnt_init(void)
{
    if (s_state == PCNT_PORT_RUNNING)
    {
        return FW_OK;
    }
    if (s_state == PCNT_PORT_FAULT)
    {
        return s_error.code;
    }
    if (s_state != PCNT_PORT_OFF)
    {
        return pcnt_cleanup_after_error(
            PCNT_FAIL(FW_ERR_PCNT_INVALID_STATE, PCNT_PORT_STAGE_VALIDATE, ESP_OK));
    }
    s_error = (fw_error_record_t){0};
    const pcnt_unit_config_t unit_config = {
        .low_limit = -PCNT_COUNT_LIMIT,
        .high_limit = PCNT_COUNT_LIMIT,
        .flags.accum_count = 1
    };
    const pcnt_chan_config_t channel_config = {
        .edge_gpio_num = BOARD_GPIO_DROP_COUNT_PIN,
        .level_gpio_num = -1
    };
    const pcnt_glitch_filter_config_t filter = {.max_glitch_ns = DROP_COUNT_GLITCH_NS};

    esp_err_t err = pcnt_new_unit(&unit_config, &s_unit);
    if (err != ESP_OK)
    {
        return PCNT_FAIL(FW_ERR_PCNT_INIT, PCNT_PORT_STAGE_UNIT, err);
    }
    err = pcnt_new_channel(s_unit, &channel_config, &s_channel);
    if (err != ESP_OK)
    {
        return pcnt_cleanup_after_error(PCNT_FAIL(FW_ERR_PCNT_INIT, PCNT_PORT_STAGE_CHANNEL, err));
    }
    err = pcnt_channel_set_edge_action(s_channel, PCNT_CHANNEL_EDGE_ACTION_HOLD,
                                       PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    if (err != ESP_OK)
    {
        return pcnt_cleanup_after_error(PCNT_FAIL(FW_ERR_PCNT_INIT, PCNT_PORT_STAGE_EDGE, err));
    }
    err = pcnt_channel_set_level_action(s_channel, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                        PCNT_CHANNEL_LEVEL_ACTION_KEEP);
    if (err != ESP_OK)
    {
        return pcnt_cleanup_after_error(PCNT_FAIL(FW_ERR_PCNT_INIT, PCNT_PORT_STAGE_EDGE, err));
    }
    err = pcnt_unit_set_glitch_filter(s_unit, &filter);
    if (err != ESP_OK)
    {
        return pcnt_cleanup_after_error(PCNT_FAIL(FW_ERR_PCNT_INIT, PCNT_PORT_STAGE_FILTER, err));
    }
    err = pcnt_unit_add_watch_point(s_unit, -PCNT_COUNT_LIMIT);
    if (err == ESP_OK)
    {
        err = pcnt_unit_add_watch_point(s_unit, PCNT_COUNT_LIMIT);
    }
    if (err != ESP_OK)
    {
        return pcnt_cleanup_after_error(PCNT_FAIL(FW_ERR_PCNT_INIT, PCNT_PORT_STAGE_LIMIT, err));
    }
    err = pcnt_unit_enable(s_unit);
    if (err != ESP_OK)
    {
        return pcnt_cleanup_after_error(PCNT_FAIL(FW_ERR_PCNT_INIT, PCNT_PORT_STAGE_ENABLE, err));
    }
    s_enabled = true;
    err = pcnt_unit_clear_count(s_unit);
    if (err != ESP_OK)
    {
        return pcnt_cleanup_after_error(PCNT_FAIL(FW_ERR_PCNT_INIT, PCNT_PORT_STAGE_CLEAR, err));
    }
    err = pcnt_unit_start(s_unit);
    if (err != ESP_OK)
    {
        return pcnt_cleanup_after_error(PCNT_FAIL(FW_ERR_PCNT_INIT, PCNT_PORT_STAGE_START, err));
    }
    s_started = true;
    s_state = PCNT_PORT_RUNNING;
    return FW_OK;
}

fw_error_t core_pcnt_read(uint32_t *count)
{
    if (count == NULL)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    if (s_state == PCNT_PORT_OFF)
    {
        return FW_ERR_PORT_NOT_READY;
    }
    if (s_state == PCNT_PORT_FAULT)
    {
        return s_error.code;
    }
    if (s_state != PCNT_PORT_RUNNING)
    {
        return pcnt_cleanup_after_error(
            PCNT_FAIL(FW_ERR_PCNT_INVALID_STATE, PCNT_PORT_STAGE_VALIDATE, ESP_OK));
    }
    int value;
    esp_err_t err = pcnt_unit_get_count(s_unit, &value);
    if (err != ESP_OK || value < 0 || value >= INT_MAX - PCNT_COUNT_LIMIT)
    {
        /* SDK累计值为有符号int；台架不跨其累计上限运行。 */
        return pcnt_cleanup_after_error(PCNT_FAIL(FW_ERR_PCNT_READ, PCNT_PORT_STAGE_READ, err));
    }
    *count = (uint32_t)value;
    return FW_OK;
}

fw_error_t core_pcnt_deinit(void)
{
    esp_err_t err = pcnt_release();
    if (err != ESP_OK)
    {
        (void)PCNT_FAIL(FW_ERR_PCNT_RELEASE, PCNT_PORT_STAGE_RELEASE, err);
        s_error.cleanup_code = FW_ERR_PCNT_RELEASE;
        return FW_ERR_PCNT_RELEASE;
    }
    s_state = PCNT_PORT_OFF;
    return FW_OK;
}

fw_error_t core_pcnt_get_error(fw_error_record_t *error)
{
    if (error == NULL)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    *error = s_error;
    return FW_OK;
}
