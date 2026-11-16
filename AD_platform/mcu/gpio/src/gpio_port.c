/**
 * @file gpio_port.c
 * @brief 实现 ESP32-S3 GPIO 平台端口、中断桥接与可定位错误快照。
 */
#include "gpio_port.h"

#include "board_config.h"
#include "bubble_adc_test_config.h"
#include "dosing_control_config.h"
#include "osal_task.h"

#include <stddef.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "driver/gpio.h"
#include "esp_err.h"
#else
typedef int32_t esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103
#endif

/** @brief 单个逻辑引脚的内部运行期槽位状态。 */
typedef struct
{
    core_gpio_pin_config_t cfg; /**< 当前生效的引脚配置。 */
    core_gpio_level_t out_lvl;  /**< 最近一次设置的输出电平。 */
    core_gpio_isr_cb_t isr_cb;  /**< 注册的外部中断回调。 */
    void *isr_arg;              /**< 外部中断回调上下文。 */
    core_gpio_irq_trig_t trig;  /**< 当前中断触发边沿。 */
    bool configured;            /**< 是否已完成底层配置。 */
    bool isr_attached;          /**< 是否已向底层挂载 ISR 处理器。 */
} gpio_slot_t;

/** @brief 板级默认 GPIO 配置表，权威引脚号来自 AF_config/board_config.h。 */
static const core_gpio_pin_config_t s_default_pin_configs[CORE_GPIO_PIN_MAX] =
{
    [CORE_GPIO_PIN_STATUS_LED] = {
#if APP_DOSING_CONTROL_ENABLE && !APP_BUBBLE_ADC_TEST_ENABLE
        .hw_pin_num = BOARD_GPIO_PIN_NC, /* WS2812由RMT独占。 */
#else
        .hw_pin_num = BOARD_GPIO_STATUS_LED_PIN,
#endif
        .direction = CORE_GPIO_DIR_INPUT_OUTPUT,
        .pull_mode = CORE_GPIO_PULL_NONE,
        .default_level = CORE_GPIO_LEVEL_LOW
    },
    [CORE_GPIO_PIN_RS485_BUS1_DE] = {
        .hw_pin_num = BOARD_GPIO_RS485_BUS1_DE_PIN,
        .direction = CORE_GPIO_DIR_INPUT_OUTPUT,
        .pull_mode = CORE_GPIO_PULL_DOWN,
        .default_level = CORE_GPIO_LEVEL_LOW
    },
    [CORE_GPIO_PIN_RS485_BUS2_DE] = {
        .hw_pin_num = BOARD_GPIO_RS485_BUS2_DE_PIN,
        .direction = CORE_GPIO_DIR_INPUT_OUTPUT,
        .pull_mode = CORE_GPIO_PULL_DOWN,
        .default_level = CORE_GPIO_LEVEL_LOW
    },
    [CORE_GPIO_PIN_SENSOR_TRIG] = {
        .hw_pin_num = BOARD_GPIO_SENSOR_TRIG_PIN,
        .direction = CORE_GPIO_DIR_INPUT,
        .pull_mode = CORE_GPIO_PULL_UP,
        .default_level = CORE_GPIO_LEVEL_LOW
    }
};

static core_gpio_state_t s_gpio_state = CORE_GPIO_STATE_UNINITIALIZED;
static fw_error_record_t s_gpio_error = {0};
static gpio_slot_t s_gpio_slots[CORE_GPIO_PIN_MAX] = {0};
static bool s_isr_service_installed = false;

/** @brief 统一记录首个 GPIO 终态失败并转入 CORE_GPIO_STATE_FAULT。 */
static fw_error_t gpio_fail_at(fw_error_t code, int32_t native_err, bool has_native,
                               core_gpio_stage_t stage, uint32_t pin_id,
                               const char *file, const char *function, uint32_t line)
{
    if (s_gpio_state == CORE_GPIO_STATE_FAULT && s_gpio_error.code != FW_OK)
    {
        return s_gpio_error.code;
    }
    fw_error_capture_at(&s_gpio_error, code, FW_OK, (uint32_t)s_gpio_state,
                        (uint16_t)stage, pin_id, osal_task_now_ms(),
                        file, function, line);
    s_gpio_error.native_status = native_err;
    s_gpio_error.native_status_valid = has_native;
    s_gpio_state = CORE_GPIO_STATE_FAULT;
    return s_gpio_error.code;
}

/** @brief 在每个实际失败点捕获源文件、函数与行号。 */
#define GPIO_FAIL(code, native_err, has_native, stage, pin_id) \
    gpio_fail_at((code), (native_err), (has_native), (stage), (pin_id), \
                 __FILE__, __func__, (uint32_t)__LINE__)

#if defined(ESP_PLATFORM)
/** @brief ESP-IDF GPIO ISR 桥接函数，运行在中断上下文。 */
static void gpio_esp_isr_bridge(void *arg)
{
    uintptr_t idx = (uintptr_t)arg;
    if (idx < (uintptr_t)CORE_GPIO_PIN_MAX)
    {
        core_gpio_isr_cb_t cb = s_gpio_slots[idx].isr_cb;
        void *user_arg = s_gpio_slots[idx].isr_arg;
        if (cb != NULL)
        {
            cb((core_gpio_pin_t)idx, user_arg);
        }
    }
}

static esp_err_t hw_gpio_apply_config(const core_gpio_pin_config_t *cfg)
{
    gpio_mode_t mode = GPIO_MODE_INPUT;
    if (cfg->direction == CORE_GPIO_DIR_OUTPUT)
    {
        mode = GPIO_MODE_OUTPUT;
    }
    else if (cfg->direction == CORE_GPIO_DIR_INPUT_OUTPUT)
    {
        mode = GPIO_MODE_INPUT_OUTPUT;
    }
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << (uint32_t)cfg->hw_pin_num),
        .mode = mode,
        .pull_up_en = (cfg->pull_mode == CORE_GPIO_PULL_UP) ?
                      GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (cfg->pull_mode == CORE_GPIO_PULL_DOWN) ?
                        GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        return err;
    }
    if (cfg->direction != CORE_GPIO_DIR_INPUT)
    {
        err = gpio_set_level((gpio_num_t)cfg->hw_pin_num,
                             (cfg->default_level == CORE_GPIO_LEVEL_HIGH) ? 1U : 0U);
    }
    return err;
}

static esp_err_t hw_gpio_write(int32_t hw_pin, core_gpio_level_t level)
{
    return gpio_set_level((gpio_num_t)hw_pin,
                          (level == CORE_GPIO_LEVEL_HIGH) ? 1U : 0U);
}

static core_gpio_level_t hw_gpio_read(int32_t hw_pin)
{
    return (gpio_get_level((gpio_num_t)hw_pin) != 0) ?
           CORE_GPIO_LEVEL_HIGH : CORE_GPIO_LEVEL_LOW;
}

static esp_err_t hw_gpio_set_dir(int32_t hw_pin, core_gpio_dir_t dir)
{
    gpio_mode_t mode = GPIO_MODE_INPUT;
    if (dir == CORE_GPIO_DIR_OUTPUT)
    {
        mode = GPIO_MODE_OUTPUT;
    }
    else if (dir == CORE_GPIO_DIR_INPUT_OUTPUT)
    {
        mode = GPIO_MODE_INPUT_OUTPUT;
    }
    return gpio_set_direction((gpio_num_t)hw_pin, mode);
}

static esp_err_t hw_gpio_attach_isr(core_gpio_pin_t pin, int32_t hw_pin,
                                    core_gpio_irq_trig_t trig)
{
    if (!s_isr_service_installed)
    {
        esp_err_t srv_err = gpio_install_isr_service(0);
        if (srv_err != ESP_OK && srv_err != ESP_ERR_INVALID_STATE)
        {
            return srv_err;
        }
        s_isr_service_installed = true;
    }
    gpio_int_type_t intr_type = GPIO_INTR_DISABLE;
    if (trig == CORE_GPIO_IRQ_RISING)
    {
        intr_type = GPIO_INTR_POSEDGE;
    }
    else if (trig == CORE_GPIO_IRQ_FALLING)
    {
        intr_type = GPIO_INTR_NEGEDGE;
    }
    else if (trig == CORE_GPIO_IRQ_ANYEDGE)
    {
        intr_type = GPIO_INTR_ANYEDGE;
    }
    esp_err_t err = gpio_set_intr_type((gpio_num_t)hw_pin, intr_type);
    if (err != ESP_OK)
    {
        return err;
    }
    if (s_gpio_slots[pin].isr_attached)
    {
        (void)gpio_isr_handler_remove((gpio_num_t)hw_pin);
        s_gpio_slots[pin].isr_attached = false;
    }
    err = gpio_isr_handler_add((gpio_num_t)hw_pin, gpio_esp_isr_bridge,
                               (void *)(uintptr_t)pin);
    if (err != ESP_OK)
    {
        return err;
    }
    return gpio_intr_enable((gpio_num_t)hw_pin);
}

static esp_err_t hw_gpio_enable_irq(int32_t hw_pin)
{
    return gpio_intr_enable((gpio_num_t)hw_pin);
}

static esp_err_t hw_gpio_disable_irq(int32_t hw_pin)
{
    return gpio_intr_disable((gpio_num_t)hw_pin);
}

static esp_err_t hw_gpio_reset_pin(core_gpio_pin_t pin, int32_t hw_pin)
{
    if (s_gpio_slots[pin].isr_attached)
    {
        (void)gpio_isr_handler_remove((gpio_num_t)hw_pin);
        s_gpio_slots[pin].isr_attached = false;
    }
    return gpio_reset_pin((gpio_num_t)hw_pin);
}
#else
/* 主机测试替身钩子：允许单测注入底层硬件错误并检查引脚电平，不改变生产状态机逻辑。 */
static esp_err_t s_host_cfg_err = ESP_OK;
static esp_err_t s_host_io_err = ESP_OK;
static esp_err_t s_host_isr_err = ESP_OK;
static core_gpio_level_t s_host_pin_levels[CORE_GPIO_PIN_MAX] = {0};

static esp_err_t hw_gpio_apply_config(const core_gpio_pin_config_t *cfg)
{
    (void)cfg;
    return s_host_cfg_err;
}

static esp_err_t hw_gpio_write(int32_t hw_pin, core_gpio_level_t level)
{
    (void)hw_pin;
    (void)level;
    return s_host_io_err;
}

static core_gpio_level_t hw_gpio_read(int32_t hw_pin)
{
    for (size_t i = 0U; i < (size_t)CORE_GPIO_PIN_MAX; ++i)
    {
        if (s_gpio_slots[i].configured && s_gpio_slots[i].cfg.hw_pin_num == hw_pin)
        {
            return s_host_pin_levels[i];
        }
    }
    return CORE_GPIO_LEVEL_LOW;
}

static esp_err_t hw_gpio_set_dir(int32_t hw_pin, core_gpio_dir_t dir)
{
    (void)hw_pin;
    (void)dir;
    return s_host_io_err;
}

static esp_err_t hw_gpio_attach_isr(core_gpio_pin_t pin, int32_t hw_pin,
                                    core_gpio_irq_trig_t trig)
{
    (void)pin;
    (void)hw_pin;
    (void)trig;
    s_isr_service_installed = true;
    return s_host_isr_err;
}

static esp_err_t hw_gpio_enable_irq(int32_t hw_pin)
{
    (void)hw_pin;
    return s_host_isr_err;
}

static esp_err_t hw_gpio_disable_irq(int32_t hw_pin)
{
    (void)hw_pin;
    return s_host_isr_err;
}

static esp_err_t hw_gpio_reset_pin(core_gpio_pin_t pin, int32_t hw_pin)
{
    (void)hw_pin;
    s_gpio_slots[pin].isr_attached = false;
    return s_host_io_err;
}
#endif

/** @brief 校验 GPIO 主状态是否处于 READY，非法枚举进入统一 FAULT 入口。 */
static fw_error_t gpio_check_ready(core_gpio_stage_t stage, uint32_t pin_id)
{
    switch (s_gpio_state)
    {
        case CORE_GPIO_STATE_READY:
            return FW_OK;
        case CORE_GPIO_STATE_UNINITIALIZED:
            return FW_ERR_GPIO_NOT_READY;
        case CORE_GPIO_STATE_FAULT:
            return s_gpio_error.code;
        default:
            return GPIO_FAIL(FW_ERR_GPIO_INVALID_STATE, 0, false, stage, pin_id);
    }
}

/** @brief 校验单个引脚配置结构体字段范围。 */
static bool gpio_config_valid(const core_gpio_pin_config_t *config)
{
    if (config == NULL || config->hw_pin_num < 0 || config->hw_pin_num > 48)
    {
        return false;
    }
    if (config->direction < CORE_GPIO_DIR_INPUT ||
        config->direction > CORE_GPIO_DIR_INPUT_OUTPUT)
    {
        return false;
    }
    if (config->pull_mode < CORE_GPIO_PULL_NONE ||
        config->pull_mode > CORE_GPIO_PULL_DOWN)
    {
        return false;
    }
    if (config->default_level != CORE_GPIO_LEVEL_LOW &&
        config->default_level != CORE_GPIO_LEVEL_HIGH)
    {
        return false;
    }
    return true;
}

fw_error_t core_gpio_init(void)
{
    memset(s_gpio_slots, 0, sizeof(s_gpio_slots));
    memset(&s_gpio_error, 0, sizeof(s_gpio_error));
    s_gpio_state = CORE_GPIO_STATE_UNINITIALIZED;

    for (uint32_t i = 0U; i < (uint32_t)CORE_GPIO_PIN_MAX; ++i)
    {
        const core_gpio_pin_config_t *cfg = &s_default_pin_configs[i];
        s_gpio_slots[i].cfg = *cfg;
        s_gpio_slots[i].out_lvl = cfg->default_level;
        if (cfg->hw_pin_num == BOARD_GPIO_PIN_NC)
        {
            s_gpio_slots[i].configured = false;
            continue;
        }
        esp_err_t err = hw_gpio_apply_config(cfg);
        if (err != ESP_OK)
        {
            /* 清理此前已配置的引脚，不覆盖首个失败原因。 */
            for (uint32_t j = 0U; j < i; ++j)
            {
                if (s_gpio_slots[j].configured)
                {
                    (void)hw_gpio_reset_pin((core_gpio_pin_t)j,
                                            s_gpio_slots[j].cfg.hw_pin_num);
                    s_gpio_slots[j].configured = false;
                }
            }
            return GPIO_FAIL(FW_ERR_GPIO_CONFIG_FAILED, (int32_t)err, true,
                             CORE_GPIO_STAGE_INIT, i);
        }
        s_gpio_slots[i].configured = true;
#if !defined(ESP_PLATFORM)
        s_host_pin_levels[i] = cfg->default_level;
#endif
    }

    s_gpio_state = CORE_GPIO_STATE_READY;
    return FW_OK;
}

fw_error_t core_gpio_deinit(void)
{
    if (s_gpio_state == CORE_GPIO_STATE_UNINITIALIZED)
    {
        return FW_OK;
    }
    for (uint32_t i = 0U; i < (uint32_t)CORE_GPIO_PIN_MAX; ++i)
    {
        if (s_gpio_slots[i].configured)
        {
            esp_err_t err = hw_gpio_reset_pin((core_gpio_pin_t)i,
                                              s_gpio_slots[i].cfg.hw_pin_num);
            if (err != ESP_OK)
            {
                return GPIO_FAIL(FW_ERR_GPIO_IO_FAILED, (int32_t)err, true,
                                 CORE_GPIO_STAGE_DEINIT, i);
            }
            s_gpio_slots[i].configured = false;
        }
    }
    s_gpio_state = CORE_GPIO_STATE_UNINITIALIZED;
    return FW_OK;
}

fw_error_t core_gpio_configure_pin(core_gpio_pin_t pin,
                                   const core_gpio_pin_config_t *config)
{
    fw_error_t ready_err = gpio_check_ready(CORE_GPIO_STAGE_CONFIG, (uint32_t)pin);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    if (pin < CORE_GPIO_PIN_STATUS_LED || pin >= CORE_GPIO_PIN_MAX ||
        !gpio_config_valid(config))
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    esp_err_t err = hw_gpio_apply_config(config);
    if (err != ESP_OK)
    {
        return GPIO_FAIL(FW_ERR_GPIO_CONFIG_FAILED, (int32_t)err, true,
                         CORE_GPIO_STAGE_CONFIG, (uint32_t)pin);
    }
    s_gpio_slots[pin].cfg = *config;
    s_gpio_slots[pin].out_lvl = config->default_level;
    s_gpio_slots[pin].configured = true;
#if !defined(ESP_PLATFORM)
    s_host_pin_levels[pin] = config->default_level;
#endif
    return FW_OK;
}

fw_error_t core_gpio_write_pin(core_gpio_pin_t pin, core_gpio_level_t level)
{
    fw_error_t ready_err = gpio_check_ready(CORE_GPIO_STAGE_IO, (uint32_t)pin);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    if (pin < CORE_GPIO_PIN_STATUS_LED || pin >= CORE_GPIO_PIN_MAX ||
        (level != CORE_GPIO_LEVEL_LOW && level != CORE_GPIO_LEVEL_HIGH))
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    if (!s_gpio_slots[pin].configured ||
        s_gpio_slots[pin].cfg.direction == CORE_GPIO_DIR_INPUT)
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    esp_err_t err = hw_gpio_write(s_gpio_slots[pin].cfg.hw_pin_num, level);
    if (err != ESP_OK)
    {
        return GPIO_FAIL(FW_ERR_GPIO_IO_FAILED, (int32_t)err, true,
                         CORE_GPIO_STAGE_IO, (uint32_t)pin);
    }
    s_gpio_slots[pin].out_lvl = level;
#if !defined(ESP_PLATFORM)
    s_host_pin_levels[pin] = level;
#endif
    return FW_OK;
}

fw_error_t core_gpio_read_pin(core_gpio_pin_t pin, core_gpio_level_t *level)
{
    fw_error_t ready_err = gpio_check_ready(CORE_GPIO_STAGE_IO, (uint32_t)pin);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    if (level == NULL || pin < CORE_GPIO_PIN_STATUS_LED ||
        pin >= CORE_GPIO_PIN_MAX || !s_gpio_slots[pin].configured)
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    *level = hw_gpio_read(s_gpio_slots[pin].cfg.hw_pin_num);
    return FW_OK;
}

fw_error_t core_gpio_toggle_pin(core_gpio_pin_t pin)
{
    fw_error_t ready_err = gpio_check_ready(CORE_GPIO_STAGE_IO, (uint32_t)pin);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    if (pin < CORE_GPIO_PIN_STATUS_LED || pin >= CORE_GPIO_PIN_MAX ||
        !s_gpio_slots[pin].configured ||
        s_gpio_slots[pin].cfg.direction == CORE_GPIO_DIR_INPUT)
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    core_gpio_level_t next =
        (s_gpio_slots[pin].out_lvl == CORE_GPIO_LEVEL_LOW) ?
        CORE_GPIO_LEVEL_HIGH : CORE_GPIO_LEVEL_LOW;
    return core_gpio_write_pin(pin, next);
}

fw_error_t core_gpio_set_direction(core_gpio_pin_t pin, core_gpio_dir_t dir)
{
    fw_error_t ready_err = gpio_check_ready(CORE_GPIO_STAGE_CONFIG, (uint32_t)pin);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    if (pin < CORE_GPIO_PIN_STATUS_LED || pin >= CORE_GPIO_PIN_MAX ||
        !s_gpio_slots[pin].configured ||
        dir < CORE_GPIO_DIR_INPUT || dir > CORE_GPIO_DIR_INPUT_OUTPUT)
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    esp_err_t err = hw_gpio_set_dir(s_gpio_slots[pin].cfg.hw_pin_num, dir);
    if (err != ESP_OK)
    {
        return GPIO_FAIL(FW_ERR_GPIO_IO_FAILED, (int32_t)err, true,
                         CORE_GPIO_STAGE_CONFIG, (uint32_t)pin);
    }
    s_gpio_slots[pin].cfg.direction = dir;
    return FW_OK;
}

fw_error_t core_gpio_register_isr(core_gpio_pin_t pin,
                                  core_gpio_irq_trig_t trig,
                                  core_gpio_isr_cb_t cb,
                                  void *arg)
{
    fw_error_t ready_err = gpio_check_ready(CORE_GPIO_STAGE_ISR, (uint32_t)pin);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    if (pin < CORE_GPIO_PIN_STATUS_LED || pin >= CORE_GPIO_PIN_MAX ||
        !s_gpio_slots[pin].configured || cb == NULL ||
        trig <= CORE_GPIO_IRQ_NONE || trig > CORE_GPIO_IRQ_ANYEDGE)
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    s_gpio_slots[pin].isr_cb = cb;
    s_gpio_slots[pin].isr_arg = arg;
    s_gpio_slots[pin].trig = trig;
    esp_err_t err = hw_gpio_attach_isr(pin, s_gpio_slots[pin].cfg.hw_pin_num, trig);
    if (err != ESP_OK)
    {
        s_gpio_slots[pin].isr_cb = NULL;
        s_gpio_slots[pin].isr_arg = NULL;
        return GPIO_FAIL(FW_ERR_GPIO_ISR_FAILED, (int32_t)err, true,
                         CORE_GPIO_STAGE_ISR, (uint32_t)pin);
    }
    s_gpio_slots[pin].isr_attached = true;
    return FW_OK;
}

fw_error_t core_gpio_enable_irq(core_gpio_pin_t pin)
{
    fw_error_t ready_err = gpio_check_ready(CORE_GPIO_STAGE_ISR, (uint32_t)pin);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    if (pin < CORE_GPIO_PIN_STATUS_LED || pin >= CORE_GPIO_PIN_MAX ||
        !s_gpio_slots[pin].configured || !s_gpio_slots[pin].isr_attached)
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    esp_err_t err = hw_gpio_enable_irq(s_gpio_slots[pin].cfg.hw_pin_num);
    if (err != ESP_OK)
    {
        return GPIO_FAIL(FW_ERR_GPIO_ISR_FAILED, (int32_t)err, true,
                         CORE_GPIO_STAGE_ISR, (uint32_t)pin);
    }
    return FW_OK;
}

fw_error_t core_gpio_disable_irq(core_gpio_pin_t pin)
{
    fw_error_t ready_err = gpio_check_ready(CORE_GPIO_STAGE_ISR, (uint32_t)pin);
    if (ready_err != FW_OK)
    {
        return ready_err;
    }
    if (pin < CORE_GPIO_PIN_STATUS_LED || pin >= CORE_GPIO_PIN_MAX ||
        !s_gpio_slots[pin].configured)
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    esp_err_t err = hw_gpio_disable_irq(s_gpio_slots[pin].cfg.hw_pin_num);
    if (err != ESP_OK)
    {
        return GPIO_FAIL(FW_ERR_GPIO_ISR_FAILED, (int32_t)err, true,
                         CORE_GPIO_STAGE_ISR, (uint32_t)pin);
    }
    return FW_OK;
}

fw_error_t core_gpio_get_status(core_gpio_state_t *state, fw_error_record_t *error)
{
    if (state == NULL || error == NULL)
    {
        return FW_ERR_GPIO_INVALID_ARG;
    }
    *state = s_gpio_state;
    *error = s_gpio_error;
    return FW_OK;
}
