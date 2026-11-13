/**
 * @file fish_bin.c
 * @brief ESP-IDF 启动入口，委托 AA_app 组合根完成初始化。
 */
#include "app_main.h"

void app_main(void)
{
    (void)app_init();
}
