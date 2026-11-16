#include "adc_port.h"

#include "board_config.h"
#include "osal_task.h"

#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"

#include <stddef.h>

typedef enum
{
    ADC_PORT_MAP = 1,
    ADC_PORT_UNIT,
    ADC_PORT_CHANNEL,
    ADC_PORT_CALIBRATION,
    ADC_PORT_CONVERSION,
    ADC_PORT_VOLTAGE,
    ADC_PORT_RELEASE
} adc_port_stage_t;

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_calibration;
static adc_channel_t s_channel;
static bool s_ready;

static fw_error_t adc_error_at(fw_error_record_t *error, fw_error_t code, adc_port_stage_t stage,
                               esp_err_t native, const char *file, const char *function,
                               uint32_t line)
{
    fw_error_capture_at(error, code, FW_OK, s_ready ? 1U : 0U, (uint16_t)stage, 0U,
                        osal_task_now_ms(), file, function, line);
    error->native_status = native;
    error->native_status_valid = true;
    return code;
}

#define ADC_ERROR(error, code, stage, native) \
    adc_error_at((error), (code), (stage), (native), __FILE__, __func__, (uint32_t)__LINE__)

fw_error_t core_adc_deinit(fw_error_record_t *error)
{
    if (error == NULL)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    *error = (fw_error_record_t){0};
    s_ready = false;
    if (s_calibration != NULL)
    {
        esp_err_t err = adc_cali_delete_scheme_curve_fitting(s_calibration);
        if (err != ESP_OK)
        {
            return ADC_ERROR(error, FW_ERR_ADC_RELEASE, ADC_PORT_RELEASE, err);
        }
        s_calibration = NULL;
    }
    if (s_adc != NULL)
    {
        esp_err_t err = adc_oneshot_del_unit(s_adc);
        if (err != ESP_OK)
        {
            return ADC_ERROR(error, FW_ERR_ADC_RELEASE, ADC_PORT_RELEASE, err);
        }
        s_adc = NULL;
    }
    return FW_OK;
}

fw_error_t core_adc_init(fw_error_record_t *error)
{
    if (error == NULL)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    *error = (fw_error_record_t){0};
    if (s_ready)
    {
        return FW_OK;
    }
    if (s_adc != NULL)
    {
        return ADC_ERROR(error, FW_ERR_ADC_INIT, ADC_PORT_RELEASE, ESP_ERR_INVALID_STATE);
    }

    adc_unit_t unit;
    esp_err_t err = adc_oneshot_io_to_channel(BOARD_GPIO_BUBBLE_PIN, &unit, &s_channel);
    if (err != ESP_OK)
    {
        return ADC_ERROR(error, FW_ERR_ADC_INIT, ADC_PORT_MAP, err);
    }
    const adc_oneshot_unit_init_cfg_t init = {.unit_id = unit};
    err = adc_oneshot_new_unit(&init, &s_adc);
    if (err != ESP_OK)
    {
        return ADC_ERROR(error, FW_ERR_ADC_INIT, ADC_PORT_UNIT, err);
    }
    const adc_oneshot_chan_cfg_t channel = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
    err = adc_oneshot_config_channel(s_adc, s_channel, &channel);
    if (err != ESP_OK)
    {
        (void)ADC_ERROR(error, FW_ERR_ADC_INIT, ADC_PORT_CHANNEL, err);
    }
    else
    {
        const adc_cali_curve_fitting_config_t calibration = {
            .unit_id = unit, .chan = s_channel, .atten = channel.atten, .bitwidth = channel.bitwidth
        };
        err = adc_cali_create_scheme_curve_fitting(&calibration, &s_calibration);
        if (err == ESP_OK || err == ESP_ERR_NOT_SUPPORTED)
        {
            s_ready = true;
            return FW_OK;
        }
        (void)ADC_ERROR(error, FW_ERR_ADC_INIT, ADC_PORT_CALIBRATION, err);
    }
    fw_error_record_t cleanup = {0};
    error->cleanup_code = core_adc_deinit(&cleanup);
    return error->code;
}

fw_error_t core_adc_read(core_adc_sample_t *sample, fw_error_record_t *error)
{
    if (sample == NULL || error == NULL)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    *error = (fw_error_record_t){0};
    if (!s_ready)
    {
        return FW_ERR_PORT_NOT_READY;
    }
    core_adc_sample_t reading = {0};
    esp_err_t err = adc_oneshot_read(s_adc, s_channel, &reading.raw);
    if (err != ESP_OK)
    {
        return ADC_ERROR(error, FW_ERR_ADC_READ, ADC_PORT_CONVERSION, err);
    }
    if (s_calibration != NULL)
    {
        err = adc_cali_raw_to_voltage(s_calibration, reading.raw, &reading.voltage_mv);
        if (err != ESP_OK)
        {
            return ADC_ERROR(error, FW_ERR_ADC_READ, ADC_PORT_VOLTAGE, err);
        }
        reading.voltage_valid = true;
    }
    *sample = reading;
    return FW_OK;
}
