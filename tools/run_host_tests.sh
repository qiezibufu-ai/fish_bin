#!/usr/bin/env bash
# 链接生产源执行主机回归；硬件时序和物理运动需另行实测。
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p work/tests
cc -std=c11 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I AA_app -I AB_service/dosing/inc -I AB_service/bubble/inc -I AF_config -I AE_common \
    -I AD_platform/os/inc -I AD_platform/os/src -I AD_platform/mcu/gpio/inc \
    -I AD_platform/mcu/uart/inc -I AD_platform/mcu/adc/inc -I AD_platform/mcu/panel/inc \
    -I tests/stubs -I AD_platform/mcu/pcnt/inc -I AC_module/dosing_inputs/wrapper/inc \
    -I AC_module/kamoer/driver/inc -I AC_module/adapter/inc \
    tests/test_layer_apis.c tests/test_motor.c tests/test_drop_count.c tests/test_dosing.c \
    AD_platform/mcu/pcnt/src/pcnt_port.c AC_module/adapter/src/bsp_adapter_port_drop_count.c \
    AA_app/app_dosing_control.c AB_service/bubble/src/service_bubble.c \
    AB_service/dosing/src/service_dosing.c AC_module/kamoer/driver/src/bsp_kamoer_driver.c \
    AC_module/adapter/src/bsp_adapter_port_kamoer.c \
    AD_platform/os/src/osal_task.c AD_platform/os/src/osal_mutex.c \
    AD_platform/os/src/osal_sem.c AD_platform/os/src/osal_queue.c \
    AD_platform/os/src/osal_timer.c AE_common/fw_error.c AE_common/ring_buffer.c \
    -o work/tests/test_layer_apis
ASAN_OPTIONS=detect_leaks=1 ./work/tests/test_layer_apis

cc -std=c11 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I AA_app -I AF_config -I AE_common -I AB_service/bubble/inc -I AB_service/dosing/inc \
    -I AC_module/dosing_inputs/wrapper/inc -I AD_platform/os/inc \
    -I AD_platform/mcu/panel/inc -I AD_platform/mcu/adc/inc \
    tests/test_dosing_control.c AA_app/app_dosing_control.c AB_service/bubble/src/service_bubble.c \
    AB_service/dosing/src/service_dosing.c AE_common/fw_error.c -o work/tests/test_dosing_control
ASAN_OPTIONS=detect_leaks=1 ./work/tests/test_dosing_control
