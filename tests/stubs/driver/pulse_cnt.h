/* 主机测试替身：仅声明生产端口使用的ESP-IDF API，目标构建使用SDK原头。 */
#ifndef TESTS_STUBS_DRIVER_PULSE_CNT_H
#define TESTS_STUBS_DRIVER_PULSE_CNT_H
#include <stdint.h>
typedef int32_t esp_err_t;
#define ESP_OK 0
typedef struct pcnt_unit *pcnt_unit_handle_t;
typedef struct pcnt_channel *pcnt_channel_handle_t;
typedef struct {
    int low_limit;
    int high_limit;
    struct { unsigned accum_count:1; } flags;
} pcnt_unit_config_t;
typedef struct {
    int edge_gpio_num;
    int level_gpio_num;
} pcnt_chan_config_t;
typedef struct { uint32_t max_glitch_ns; } pcnt_glitch_filter_config_t;
typedef enum {
    PCNT_CHANNEL_EDGE_ACTION_HOLD,
    PCNT_CHANNEL_EDGE_ACTION_INCREASE
} pcnt_channel_edge_action_t;
typedef enum { PCNT_CHANNEL_LEVEL_ACTION_KEEP } pcnt_channel_level_action_t;
esp_err_t pcnt_new_unit(const pcnt_unit_config_t *config, pcnt_unit_handle_t *unit);
esp_err_t pcnt_new_channel(pcnt_unit_handle_t unit, const pcnt_chan_config_t *config,
                           pcnt_channel_handle_t *channel);
esp_err_t pcnt_channel_set_edge_action(pcnt_channel_handle_t channel,
                                       pcnt_channel_edge_action_t pos, pcnt_channel_edge_action_t neg);
esp_err_t pcnt_channel_set_level_action(pcnt_channel_handle_t channel,
                                        pcnt_channel_level_action_t high,
                                        pcnt_channel_level_action_t low);
esp_err_t pcnt_unit_set_glitch_filter(pcnt_unit_handle_t unit,
                                      const pcnt_glitch_filter_config_t *config);
esp_err_t pcnt_unit_add_watch_point(pcnt_unit_handle_t unit, int point);
esp_err_t pcnt_unit_enable(pcnt_unit_handle_t unit);
esp_err_t pcnt_unit_clear_count(pcnt_unit_handle_t unit);
esp_err_t pcnt_unit_start(pcnt_unit_handle_t unit);
esp_err_t pcnt_unit_get_count(pcnt_unit_handle_t unit, int *count);
esp_err_t pcnt_unit_stop(pcnt_unit_handle_t unit);
esp_err_t pcnt_unit_disable(pcnt_unit_handle_t unit);
esp_err_t pcnt_del_channel(pcnt_channel_handle_t channel);
esp_err_t pcnt_del_unit(pcnt_unit_handle_t unit);
#endif
