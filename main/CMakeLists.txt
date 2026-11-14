idf_component_register(
    SRCS
        "fish_bin.c"
        "../AA_app/app_main.c"
        "../AA_app/app_motor_test.c"
        "../AB_service/dosing/src/service_dosing.c"
        "../AC_module/kamoer/driver/src/bsp_kamoer_driver.c"
        "../AC_module/adapter/src/bsp_adapter_port_kamoer.c"
        "../AC_module/adapter/src/bsp_adapter_port_drop_count.c"
        "../AD_platform/mcu/pcnt/src/pcnt_port.c"
        "../AD_platform/os/src/osal_task.c"
        "../AD_platform/os/src/osal_mutex.c"
        "../AD_platform/os/src/osal_sem.c"
        "../AD_platform/os/src/osal_queue.c"
        "../AD_platform/os/src/osal_timer.c"
        "../AD_platform/os/src/os_freertos_task.c"
        "../AD_platform/os/src/os_freertos_mutex.c"
        "../AD_platform/os/src/os_freertos_sem.c"
        "../AD_platform/os/src/os_freertos_queue.c"
        "../AD_platform/os/src/os_freertos_timer.c"
        "../AD_platform/os/src/os_freertos_diagnostics.c"
        "../AD_platform/mcu/gpio/src/gpio_port.c"
        "../AD_platform/mcu/uart/src/uart_port.c"
        "../AE_common/fw_error.c"
        "../AE_common/ring_buffer.c"
    INCLUDE_DIRS
        "."
        "../AA_app"
        "../AB_service/dosing/inc"
        "../AC_module/kamoer/driver/inc"
        "../AC_module/adapter/inc"
        "../AC_module/dosing_inputs/wrapper/inc"
        "../AD_platform/os/inc"
        "../AD_platform/mcu/gpio/inc"
        "../AD_platform/mcu/pcnt/inc"
        "../AD_platform/mcu/uart/inc"
        "../AE_common"
        "../AF_config"
    PRIV_INCLUDE_DIRS
        "../AD_platform/os/src"
    PRIV_REQUIRES
        freertos
        esp_driver_gpio
        esp_driver_uart
        esp_driver_pcnt
        log
)

# 可通过 idf.py -DAPP_DOSING_TEST_ENABLE=1 build 选择清水联调镜像。
if(DEFINED APP_DOSING_TEST_ENABLE)
    if(NOT APP_DOSING_TEST_ENABLE MATCHES "^[01]$")
        message(FATAL_ERROR "APP_DOSING_TEST_ENABLE must be 0 or 1")
    endif()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        APP_DOSING_TEST_ENABLE=${APP_DOSING_TEST_ENABLE})
endif()
