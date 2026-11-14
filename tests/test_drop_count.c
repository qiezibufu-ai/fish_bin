/* 链接生产PCNT端口与adapter，以SDK替身验证生命周期和错误路径。 */
#include "board_config.h"
#include "bsp_adapter_port_drop_count.h"
#include "driver/pulse_cnt.h"
#include "drop_count_test_config.h"
#include "pcnt_port.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

struct pcnt_unit { bool allocated, enabled, started; int count; unsigned limits; };
struct pcnt_channel { bool allocated; pcnt_channel_edge_action_t pos, neg; };
static struct pcnt_unit s_hw;
static struct pcnt_channel s_input;
static bool s_fail_filter;
static bool s_fail_read;
static bool s_fail_release;

esp_err_t pcnt_new_unit(const pcnt_unit_config_t *cfg, pcnt_unit_handle_t *unit)
{
    assert(!s_hw.allocated && cfg->flags.accum_count);
    assert(cfg->low_limit == -32767 && cfg->high_limit == 32767);
    s_hw = (struct pcnt_unit){.allocated = true};
    *unit = &s_hw;
    return ESP_OK;
}

esp_err_t pcnt_new_channel(pcnt_unit_handle_t unit, const pcnt_chan_config_t *cfg,
                           pcnt_channel_handle_t *channel)
{
    assert(unit == &s_hw && !s_input.allocated);
    assert(cfg->edge_gpio_num == BOARD_GPIO_DROP_COUNT_PIN && cfg->level_gpio_num == -1);
    s_input.allocated = true;
    *channel = &s_input;
    return ESP_OK;
}

esp_err_t pcnt_channel_set_edge_action(pcnt_channel_handle_t channel,
                                       pcnt_channel_edge_action_t pos,
                                       pcnt_channel_edge_action_t neg)
{
    assert(channel == &s_input && s_input.allocated);
    s_input.pos = pos;
    s_input.neg = neg;
    return ESP_OK;
}

esp_err_t pcnt_channel_set_level_action(pcnt_channel_handle_t channel,
                                        pcnt_channel_level_action_t high,
                                        pcnt_channel_level_action_t low)
{
    assert(channel == &s_input && high == PCNT_CHANNEL_LEVEL_ACTION_KEEP && high == low);
    return ESP_OK;
}

esp_err_t pcnt_unit_set_glitch_filter(pcnt_unit_handle_t unit,
                                      const pcnt_glitch_filter_config_t *cfg)
{
    assert(unit == &s_hw && !s_hw.enabled && cfg->max_glitch_ns == DROP_COUNT_GLITCH_NS);
    return s_fail_filter ? -11 : ESP_OK;
}

esp_err_t pcnt_unit_add_watch_point(pcnt_unit_handle_t unit, int point)
{
    assert(unit == &s_hw);
    s_hw.limits |= point == -32767 ? 1U : (point == 32767 ? 2U : 4U);
    return ESP_OK;
}

esp_err_t pcnt_unit_enable(pcnt_unit_handle_t unit)
{
    assert(unit == &s_hw && s_hw.limits == 3U);
    s_hw.enabled = true;
    return ESP_OK;
}

esp_err_t pcnt_unit_clear_count(pcnt_unit_handle_t unit)
{
    assert(unit == &s_hw && s_hw.enabled && !s_hw.started);
    s_hw.count = 0;
    return ESP_OK;
}

esp_err_t pcnt_unit_start(pcnt_unit_handle_t unit)
{
    assert(unit == &s_hw && s_hw.enabled);
    s_hw.started = true;
    return ESP_OK;
}

esp_err_t pcnt_unit_get_count(pcnt_unit_handle_t unit, int *count)
{
    assert(unit == &s_hw && s_hw.started);
    if (s_fail_read)
    {
        return -12;
    }
    *count = s_hw.count;
    return ESP_OK;
}

esp_err_t pcnt_unit_stop(pcnt_unit_handle_t unit)
{
    assert(unit == &s_hw && s_hw.started);
    s_hw.started = false;
    return ESP_OK;
}

esp_err_t pcnt_unit_disable(pcnt_unit_handle_t unit)
{
    assert(unit == &s_hw && s_hw.enabled && !s_hw.started);
    s_hw.enabled = false;
    return ESP_OK;
}

esp_err_t pcnt_del_channel(pcnt_channel_handle_t channel)
{
    assert(channel == &s_input && s_input.allocated && !s_hw.enabled);
    s_input.allocated = false;
    return ESP_OK;
}

esp_err_t pcnt_del_unit(pcnt_unit_handle_t unit)
{
    assert(unit == &s_hw && !s_input.allocated && !s_hw.enabled);
    if (s_fail_release)
    {
        return -13;
    }
    s_hw.allocated = false;
    return ESP_OK;
}

void test_drop_count_simulate_edge(bool rising)
{
    assert(s_hw.started);
    pcnt_channel_edge_action_t action = rising ? s_input.pos : s_input.neg;
    if (action == PCNT_CHANNEL_EDGE_ACTION_INCREASE)
    {
        ++s_hw.count;
    }
}

void test_drop_count(void)
{
    bsp_dosing_inputs_snapshot_t snapshot = {.drop_count = 41U};
    fw_error_record_t error;
    assert(bsp_adapter_port_drop_count_read(NULL, &snapshot) == FW_ERR_PORT_NOT_READY);
    assert(snapshot.drop_count == 41U);
    s_fail_filter = true;
    s_fail_release = true;
    assert(core_pcnt_init() == FW_ERR_PCNT_INIT);
    assert(core_pcnt_get_error(&error) == FW_OK);
    assert(error.native_status == -11 && error.line > 0U);
    assert(error.cleanup_code == FW_ERR_PCNT_RELEASE);
    assert(core_pcnt_init() == FW_ERR_PCNT_INIT);
    assert(core_pcnt_deinit() == FW_ERR_PCNT_RELEASE);
    s_fail_filter = false;
    s_fail_release = false;
    assert(core_pcnt_deinit() == FW_OK);

    assert(core_pcnt_init() == FW_OK);
    assert(bsp_adapter_port_drop_count_read(NULL, &snapshot) == FW_OK);
    assert(snapshot.drop_count == 0U && snapshot.count_valid && !snapshot.liquid_valid);
    for (unsigned i = 0; i < 10U; ++i)
    {
        test_drop_count_simulate_edge(false);
        test_drop_count_simulate_edge(true);
    }
    assert(core_pcnt_init() == FW_OK); /* 幂等初始化不清零。 */
    assert(bsp_adapter_port_drop_count_read(NULL, &snapshot) == FW_OK);
    assert(snapshot.drop_count == 10U);
    assert(bsp_adapter_port_drop_count_read(NULL, &snapshot) == FW_OK);
    assert(snapshot.drop_count == 10U); /* 只读采样。 */

    s_fail_read = true;
    assert(bsp_adapter_port_drop_count_read(NULL, &snapshot) == FW_ERR_PCNT_READ);
    assert(snapshot.drop_count == 10U && !s_hw.allocated);
    assert(core_pcnt_get_error(&error) == FW_OK);
    assert(error.native_status == -12 && error.native_status_valid);
    s_fail_read = false;
    assert(core_pcnt_deinit() == FW_OK);

    assert(core_pcnt_init() == FW_OK);
    s_hw.count = INT_MAX;
    assert(bsp_adapter_port_drop_count_read(NULL, &snapshot) == FW_ERR_PCNT_READ);
    assert(snapshot.drop_count == 10U);
    assert(core_pcnt_deinit() == FW_OK);
    puts("PASS drop counter: falling edge only, snapshot, first fault and cleanup");
}
