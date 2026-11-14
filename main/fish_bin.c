/**
 * @file fish_bin.c
 * @brief ESP-IDF 启动入口，委托 AA_app 组合根完成初始化。
 */
#include "app_main.h"
#include "esp_log.h"

void app_main(void)
{
    fw_error_t err = app_init();
    if (err != FW_OK)
    {
        app_state_t state;
        fw_error_record_t record = {0};
        (void)app_get_status(&state, &record);
        ESP_LOGE("fish_bin", "init=%s stage=%u cause=%s native=%ld cleanup=%s",
                 fw_error_code_name(err), (unsigned)record.stage,
                 fw_error_code_name(record.cause_code), (long)record.native_status,
                 fw_error_code_name(record.cleanup_code));
    }
}
