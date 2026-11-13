/**
 * @file test_layer_apis.c
 * @brief 链接真实生产源验证 AE_common、AD_platform(OSAL/GPIO/UART) 与 AA_app 各层 API 契约与错误定位。
 */
#include "app_main.h"
#include "bsp_adapter_port_kamoer.h"
#include "board_config.h"
#include "fw_error.h"
#include "gpio_port.h"
#include "osal.h"
#include "platform_error.h"
#include "ring_buffer.h"
#include "uart_port.h"
#include "usart_port.h"

#include "os_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 直接包含生产源文件以验证白盒故障注入与非法状态分支，不复制被测实现。 */
#include "../AD_platform/mcu/gpio/src/gpio_port.c"
#include "../AD_platform/mcu/uart/src/uart_port.c"
#include "../AA_app/app_main.c"

/* ==================== 主机确定性 OSAL 后端替身 ============================ */

static bool s_host_in_isr = false;
static uint32_t s_host_tick = 100U;
static void (*s_host_delay_hook)(void) = NULL;
static osal_status_t s_host_task_create_err = OSAL_OK;

struct osal_mutex_obj
{
    bool locked;
};

struct osal_sema_obj
{
    uint32_t count;
    uint32_t max_count;
};

struct osal_queue_obj
{
    uint8_t storage[256];
    size_t depth;
    size_t item_size;
    size_t count;
};

struct osal_timer_obj
{
    osal_tick_type_t period_ms;
    uint8_t auto_reload;
    osal_timer_cb_function_t cb;
    void *arg;
    bool running;
};

static struct osal_mutex_obj s_mutex_pool[4];
static struct osal_sema_obj s_sema_pool[4];
static struct osal_queue_obj s_queue_pool[4];
static struct osal_timer_obj s_timer_pool[4];
static int s_dummy_task_obj = 1;

osal_status_t os_mutex_create(osal_mutex_handle_t *mutex_handle)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    s_mutex_pool[0].locked = false;
    *mutex_handle = &s_mutex_pool[0];
    return OSAL_OK;
}

osal_status_t os_mutex_delete(osal_mutex_handle_t mutex_handle)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    mutex_handle->locked = false;
    return OSAL_OK;
}

osal_status_t os_mutex_give(osal_mutex_handle_t mutex_handle)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    if (!mutex_handle->locked)
    {
        return OSAL_ERR_STATE;
    }
    mutex_handle->locked = false;
    return OSAL_OK;
}

osal_status_t os_mutex_take(osal_mutex_handle_t mutex_handle,
                            osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    if (mutex_handle->locked)
    {
        return OSAL_ERR_TIMEOUT;
    }
    mutex_handle->locked = true;
    return OSAL_OK;
}

osal_status_t os_sema_binary_create(osal_sema_handle_t *sema_handle)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    s_sema_pool[0].count = 0U;
    s_sema_pool[0].max_count = 1U;
    *sema_handle = &s_sema_pool[0];
    return OSAL_OK;
}

osal_status_t os_sema_counting_create(osal_sema_handle_t *sema_handle,
                                      uint32_t max_count,
                                      uint32_t init_count)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    s_sema_pool[1].count = init_count;
    s_sema_pool[1].max_count = max_count;
    *sema_handle = &s_sema_pool[1];
    return OSAL_OK;
}

osal_status_t os_sema_delete(osal_sema_handle_t sema_handle)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    sema_handle->count = 0U;
    return OSAL_OK;
}

osal_status_t os_sema_give(osal_sema_handle_t sema_handle)
{
    if (sema_handle->count >= sema_handle->max_count)
    {
        return OSAL_ERR_STATE;
    }
    sema_handle->count++;
    return OSAL_OK;
}

osal_status_t os_sema_take(osal_sema_handle_t sema_handle,
                           osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    if (sema_handle->count == 0U)
    {
        return OSAL_ERR_TIMEOUT;
    }
    sema_handle->count--;
    return OSAL_OK;
}

osal_status_t os_queue_create(size_t queue_depth,
                              size_t data_size,
                              osal_queue_handle_t *queue_handle)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    if (queue_depth * data_size > sizeof(s_queue_pool[0].storage))
    {
        return OSAL_ERR_NO_MEMORY;
    }
    s_queue_pool[0].depth = queue_depth;
    s_queue_pool[0].item_size = data_size;
    s_queue_pool[0].count = 0U;
    *queue_handle = &s_queue_pool[0];
    return OSAL_OK;
}

osal_status_t os_queue_delete(osal_queue_handle_t queue_handle)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    queue_handle->count = 0U;
    return OSAL_OK;
}

osal_status_t os_queue_send(osal_queue_handle_t queue_handle,
                            const void *data,
                            osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    if (queue_handle->count >= queue_handle->depth)
    {
        return OSAL_ERR_BUSY;
    }
    memcpy(&queue_handle->storage[queue_handle->count * queue_handle->item_size],
           data, queue_handle->item_size);
    queue_handle->count++;
    return OSAL_OK;
}

osal_status_t os_queue_receive(osal_queue_handle_t queue_handle,
                               void *data,
                               osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    if (queue_handle->count == 0U)
    {
        return OSAL_ERR_TIMEOUT;
    }
    memcpy(data, queue_handle->storage, queue_handle->item_size);
    queue_handle->count--;
    if (queue_handle->count > 0U)
    {
        memmove(queue_handle->storage,
                &queue_handle->storage[queue_handle->item_size],
                queue_handle->count * queue_handle->item_size);
    }
    return OSAL_OK;
}

osal_status_t os_queue_peek(osal_queue_handle_t queue_handle,
                            void *data,
                            osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    if (queue_handle->count == 0U)
    {
        return OSAL_ERR_TIMEOUT;
    }
    memcpy(data, queue_handle->storage, queue_handle->item_size);
    return OSAL_OK;
}

osal_status_t os_queue_count(osal_queue_handle_t queue_handle,
                             uint32_t *count)
{
    *count = (uint32_t)queue_handle->count;
    return OSAL_OK;
}

osal_status_t os_queue_reset(osal_queue_handle_t queue_handle)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    queue_handle->count = 0U;
    return OSAL_OK;
}

osal_status_t os_timer_create(osal_timer_handle_t *timer_handle,
                              const char *timer_name,
                              osal_tick_type_t timer_period_ms,
                              uint8_t auto_reload,
                              osal_timer_cb_function_t timer_cb,
                              void *arg)
{
    (void)timer_name;
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    s_timer_pool[0].period_ms = timer_period_ms;
    s_timer_pool[0].auto_reload = auto_reload;
    s_timer_pool[0].cb = timer_cb;
    s_timer_pool[0].arg = arg;
    s_timer_pool[0].running = false;
    *timer_handle = &s_timer_pool[0];
    return OSAL_OK;
}

osal_status_t os_timer_start(osal_timer_handle_t timer_handle,
                             osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    timer_handle->running = true;
    return OSAL_OK;
}

osal_status_t os_timer_stop(osal_timer_handle_t timer_handle,
                            osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    timer_handle->running = false;
    return OSAL_OK;
}

osal_status_t os_timer_period_change(osal_timer_handle_t timer_handle,
                                     osal_tick_type_t new_period_ms,
                                     osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    timer_handle->period_ms = new_period_ms;
    timer_handle->running = true;
    return OSAL_OK;
}

osal_status_t os_timer_reset(osal_timer_handle_t timer_handle,
                             osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    timer_handle->running = true;
    return OSAL_OK;
}

osal_status_t os_timer_delete(osal_timer_handle_t timer_handle,
                              osal_tick_type_t timeout_ms)
{
    (void)timeout_ms;
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    timer_handle->running = false;
    timer_handle->cb = NULL;
    return OSAL_OK;
}

osal_tick_type_t os_timer_period_get(osal_timer_handle_t timer_handle)
{
    return timer_handle->period_ms;
}

osal_status_t os_task_create(const char *task_name,
                             osal_task_entry entry,
                             size_t stack_size,
                             osal_priority_t priority,
                             osal_task_handle_t *task_handle,
                             void *argument)
{
    (void)task_name;
    (void)entry;
    (void)stack_size;
    (void)priority;
    (void)argument;
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    if (s_host_task_create_err != OSAL_OK)
    {
        return s_host_task_create_err;
    }
    if (task_handle != NULL)
    {
        *task_handle = (osal_task_handle_t)&s_dummy_task_obj;
    }
    return OSAL_OK;
}

osal_status_t os_task_delete(osal_task_handle_t task_handle)
{
    (void)task_handle;
    return s_host_in_isr ? OSAL_ERR_NOT_SUPPORTED : OSAL_OK;
}

osal_status_t os_task_start(void)
{
    return s_host_in_isr ? OSAL_ERR_NOT_SUPPORTED : OSAL_OK;
}

osal_status_t os_task_suspend(osal_task_handle_t task_handle)
{
    (void)task_handle;
    return s_host_in_isr ? OSAL_ERR_NOT_SUPPORTED : OSAL_OK;
}

osal_status_t os_task_resume(osal_task_handle_t task_handle)
{
    (void)task_handle;
    return OSAL_OK;
}

osal_status_t os_task_suspend_all(void)
{
    return s_host_in_isr ? OSAL_ERR_NOT_SUPPORTED : OSAL_OK;
}

osal_status_t os_task_delay(uint32_t ticks)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    s_host_tick += ticks;
    return OSAL_OK;
}

osal_status_t os_task_delay_ms(uint32_t delay_ms)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    s_host_tick += delay_ms;
    if (s_host_delay_hook != NULL)
    {
        s_host_delay_hook();
    }
    return OSAL_OK;
}

osal_status_t os_task_wait_until(osal_tick_type_t *last_wake,
                                 uint32_t period_ms)
{
    if (s_host_in_isr)
    {
        return OSAL_ERR_NOT_SUPPORTED;
    }
    *last_wake += period_ms;
    s_host_tick = *last_wake;
    return OSAL_OK;
}

osal_status_t os_critical_enter(void)
{
    return s_host_in_isr ? OSAL_ERR_NOT_SUPPORTED : OSAL_OK;
}

osal_status_t os_critical_exit(void)
{
    return s_host_in_isr ? OSAL_ERR_NOT_SUPPORTED : OSAL_OK;
}

osal_status_t os_task_yield(void)
{
    return s_host_in_isr ? OSAL_ERR_NOT_SUPPORTED : OSAL_OK;
}

osal_status_t os_irq_enable(void)
{
    return s_host_in_isr ? OSAL_ERR_NOT_SUPPORTED : OSAL_OK;
}

osal_status_t os_irq_disable(void)
{
    return s_host_in_isr ? OSAL_ERR_NOT_SUPPORTED : OSAL_OK;
}

osal_tick_type_t os_task_get_tick(void)
{
    return s_host_tick;
}

uint32_t os_task_elapsed_ms(osal_tick_type_t now, osal_tick_type_t old)
{
    int32_t delta = (int32_t)(now - old);
    return (delta <= 0) ? 0U : (uint32_t)delta;
}

uint32_t os_task_now_ms(void)
{
    return s_host_tick;
}

osal_task_handle_t os_task_get_current(void)
{
    return (osal_task_handle_t)&s_dummy_task_obj;
}

/* ==================== 1. AE_common 测试 ==================================== */

static void test_fw_error_and_ring_buffer(void)
{
    const fw_error_t all_codes[] = {
        FW_OK,
        FW_ERR_PORT_TIMEOUT, FW_ERR_PORT_BUSY, FW_ERR_PORT_IO,
        FW_ERR_PORT_INVALID_ARG, FW_ERR_PORT_NOT_READY,
        FW_ERR_PORT_NO_RESOURCE, FW_ERR_PORT_NOT_SUPPORTED,
        FW_ERR_OSAL_INVALID_ARG, FW_ERR_OSAL_TIMEOUT, FW_ERR_OSAL_NO_MEMORY,
        FW_ERR_OSAL_BUSY, FW_ERR_OSAL_NOT_SUPPORTED, FW_ERR_OSAL_INVALID_STATE,
        FW_ERR_OSAL_FAIL,
        FW_ERR_GPIO_INVALID_ARG, FW_ERR_GPIO_NOT_READY, FW_ERR_GPIO_CONFIG_FAILED,
        FW_ERR_GPIO_IO_FAILED, FW_ERR_GPIO_ISR_FAILED, FW_ERR_GPIO_INVALID_STATE,
        FW_ERR_UART_INVALID_ARG, FW_ERR_UART_NOT_READY, FW_ERR_UART_BUSY,
        FW_ERR_UART_TIMEOUT, FW_ERR_UART_CONFIG_FAILED, FW_ERR_UART_DRIVER_FAILED,
        FW_ERR_UART_TX_FAILED, FW_ERR_UART_RX_FAILED, FW_ERR_UART_INVALID_STATE,
        FW_ERR_RINGBUF_INVALID_ARG, FW_ERR_RINGBUF_FULL, FW_ERR_RINGBUF_EMPTY,
        FW_ERR_SENSOR_INVALID_ARG, FW_ERR_SENSOR_NOT_READY, FW_ERR_SENSOR_BUSY,
        FW_ERR_SENSOR_ID_READ_FAILED, FW_ERR_SENSOR_ID_MISMATCH,
        FW_ERR_SENSOR_TRIGGER_UNKNOWN, FW_ERR_SENSOR_READ_FAILED,
        FW_ERR_SENSOR_SAMPLE_TIMEOUT, FW_ERR_SENSOR_CRC, FW_ERR_SENSOR_INVALID_STATE,
        FW_ERR_APP_INVALID_ARG, FW_ERR_APP_NOT_READY, FW_ERR_APP_ALREADY_INIT,
        FW_ERR_APP_PLATFORM_INIT_FAILED, FW_ERR_APP_TASK_CREATE_FAILED,
        FW_ERR_APP_INVALID_STATE,
        FW_ERR_KAMOER_INVALID_ARG, FW_ERR_KAMOER_NOT_READY, FW_ERR_KAMOER_BUSY,
        FW_ERR_KAMOER_IO, FW_ERR_KAMOER_TIMEOUT, FW_ERR_KAMOER_CRC,
        FW_ERR_KAMOER_RESPONSE, FW_ERR_KAMOER_EXCEPTION,
        FW_ERR_KAMOER_EXECUTION_UNKNOWN, FW_ERR_KAMOER_INVALID_STATE,
        FW_ERR_APP_MOTOR_TEST_FAILED, FW_ERR_APP_MOTOR_INVALID_STATE
    };

    for (size_t i = 0U; i < sizeof(all_codes) / sizeof(all_codes[0]); ++i)
    {
        assert(strcmp(fw_error_code_name(all_codes[i]), "FW_ERR_UNKNOWN") != 0);
        assert(strcmp(fw_error_code_text_zh(all_codes[i]), "未注册错误码") != 0);
        for (size_t j = i + 1U; j < sizeof(all_codes) / sizeof(all_codes[0]); ++j)
        {
            assert(all_codes[i] != all_codes[j]);
        }
    }
    assert(strcmp(fw_error_code_name(0xDEADFFFFU), "FW_ERR_UNKNOWN") == 0);
    assert(strcmp(fw_error_code_text_zh(0xDEADFFFFU), "未注册错误码") == 0);

    /* 环形缓冲区边界与部分读写契约验证 */
    uint8_t mem[4] = {0};
    ring_buffer_t rb = {0};
    assert(ring_buffer_init(NULL, mem, 4U) == FW_ERR_RINGBUF_INVALID_ARG);
    assert(ring_buffer_init(&rb, NULL, 4U) == FW_ERR_RINGBUF_INVALID_ARG);
    assert(ring_buffer_init(&rb, mem, 0U) == FW_ERR_RINGBUF_INVALID_ARG);
    assert(ring_buffer_init(&rb, mem, sizeof(mem)) == FW_OK);
    assert(ring_buffer_is_empty(&rb) && !ring_buffer_is_full(&rb));

    const uint8_t tx[5] = {10U, 20U, 30U, 40U, 50U};
    uint32_t written = 0U;
    assert(ring_buffer_write_bytes(&rb, tx, 5U, &written) == FW_ERR_RINGBUF_FULL);
    assert(written == 4U && ring_buffer_is_full(&rb) && ring_buffer_get_free(&rb) == 0U);

    uint8_t rx[5] = {0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU};
    uint32_t read_cnt = 0U;
    assert(ring_buffer_read_bytes(&rb, rx, 5U, &read_cnt) == FW_ERR_RINGBUF_EMPTY);
    assert(read_cnt == 4U && rx[0] == 10U && rx[3] == 40U && rx[4] == 0xAAU);
    assert(ring_buffer_is_empty(&rb));
    puts("PASS AE_common: unique fw_error registry and ring_buffer boundary contracts");
}

/* ==================== 2. AD_platform/os (OSAL) 测试 ======================== */

static void dummy_timer_cb(osal_timer_handle_t timer_handle, void *argument)
{
    (void)timer_handle;
    (void)argument;
}

static void test_osal_apis(void)
{
    s_host_in_isr = false;
    osal_task_handle_t task_h = NULL;
    assert(osal_task_create(NULL, bootstrap_task_entry, 1024U, 3U, &task_h, NULL) == OSAL_ERR_PARAM);
    assert(osal_task_create("0123456789ABCDEF", bootstrap_task_entry, 1024U, 3U, &task_h, NULL) == OSAL_ERR_PARAM);
    assert(osal_task_create("ok_task", bootstrap_task_entry, 1024U, 3U, &task_h, NULL) == OSAL_OK);
    assert(task_h != NULL);
    assert(osal_task_delay_ms(OSAL_MAX_DELAY) == OSAL_ERR_PARAM);

    /* tick 回绕计算验证 */
    uint32_t elapsed = osal_task_elapsed_ms(5U, UINT32_MAX - 4U);
    assert(elapsed == 10U);

    /* Mutex 生命周期与 ISR 限制 */
    osal_mutex_handle_t mtx = NULL;
    assert(osal_mutex_create(NULL) == OSAL_ERR_PARAM);
    assert(osal_mutex_create(&mtx) == OSAL_OK && mtx != NULL);
    assert(osal_mutex_take(mtx, 10U) == OSAL_OK);
    assert(osal_mutex_take(mtx, 10U) == OSAL_ERR_TIMEOUT);
    assert(osal_mutex_give(mtx) == OSAL_OK);
    assert(osal_mutex_give(mtx) == OSAL_ERR_STATE);
    s_host_in_isr = true;
    assert(osal_mutex_take(mtx, 0U) == OSAL_ERR_NOT_SUPPORTED);
    s_host_in_isr = false;
    assert(osal_mutex_delete(mtx) == OSAL_OK);

    /* Semaphore 二值与计数验证 */
    osal_sema_handle_t sem = NULL;
    assert(osal_sema_binary_create(&sem) == OSAL_OK);
    assert(osal_sema_take(sem, 5U) == OSAL_ERR_TIMEOUT);
    assert(osal_sema_give(sem) == OSAL_OK);
    assert(osal_sema_give(sem) == OSAL_ERR_STATE);
    assert(osal_sema_take(sem, 5U) == OSAL_OK);
    assert(osal_sema_delete(sem) == OSAL_OK);

    assert(osal_sema_counting_create(&sem, 0U, 0U) == OSAL_ERR_PARAM);
    assert(osal_sema_counting_create(&sem, 2U, 3U) == OSAL_ERR_PARAM);
    assert(osal_sema_countings_create(&sem, 2U, 1U) == OSAL_OK);
    assert(osal_sema_give(sem) == OSAL_OK);
    assert(osal_sema_give(sem) == OSAL_ERR_STATE);
    assert(osal_sema_delete(sem) == OSAL_OK);

    /* Queue 发送、查看、接收与计数验证 */
    osal_queue_handle_t q = NULL;
    assert(osal_queue_create(0U, 4U, &q) == OSAL_ERR_PARAM);
    assert(osal_queue_create(2U, sizeof(uint32_t), &q) == OSAL_OK);
    uint32_t v1 = 0x12345678U, v2 = 0x87654321U, out = 0U, cnt = 0U;
    assert(osal_queue_send(q, &v1, 10U) == OSAL_OK);
    assert(osal_queue_send(q, &v2, 10U) == OSAL_OK);
    assert(osal_queue_send(q, &v1, 10U) == OSAL_ERR_BUSY);
    assert(osal_queue_count(q, &cnt) == OSAL_OK && cnt == 2U);
    assert(osal_queue_peek(q, &out, 0U) == OSAL_OK && out == v1);
    assert(osal_queue_receive(q, &out, 0U) == OSAL_OK && out == v1);
    assert(osal_queue_reset(q) == OSAL_OK);
    assert(osal_queue_count(q, &cnt) == OSAL_OK && cnt == 0U);
    assert(osal_queue_delete(q) == OSAL_OK);

    /* Timer 生命周期与参数校验 */
    osal_timer_handle_t tmr = NULL;
    assert(osal_timer_create(&tmr, "tmr", 0U, 1U, dummy_timer_cb, NULL) == OSAL_ERR_PARAM);
    assert(osal_timer_create(&tmr, "tmr", 50U, 1U, dummy_timer_cb, NULL) == OSAL_OK);
    assert(osal_timer_period_get(tmr) == 50U);
    assert(osal_timer_start(tmr, 10U) == OSAL_OK);
    assert(osal_timer_period_change(tmr, 100U, 10U) == OSAL_OK);
    assert(osal_timer_period_get(tmr) == 100U);
    assert(osal_timer_stop(tmr, 10U) == OSAL_OK);
    assert(osal_timer_delete(tmr, 10U) == OSAL_OK);
    puts("PASS AD_platform/os: task, mutex, semaphore, queue, timer and tick wrap");
}

/* ==================== 3. AD_platform/mcu GPIO 与 UART 测试 ================= */

static unsigned s_gpio_isr_fired = 0U;
static void test_gpio_isr_cb(core_gpio_pin_t pin, void *arg)
{
    uint32_t *val = (uint32_t *)arg;
    if (pin == CORE_GPIO_PIN_SENSOR_TRIG && val != NULL)
    {
        s_gpio_isr_fired += *val;
    }
}

static void test_gpio_and_uart_ports(void)
{
    s_host_cfg_err = ESP_OK;
    s_host_io_err = ESP_OK;
    s_host_isr_err = ESP_OK;

    /* 未初始化时拒绝操作 */
    core_gpio_level_t lvl = CORE_GPIO_LEVEL_HIGH;
    assert(core_gpio_write_pin(CORE_GPIO_PIN_STATUS_LED, CORE_GPIO_LEVEL_HIGH) == FW_ERR_GPIO_NOT_READY);

    /* GPIO 正常初始化、翻转、ISR 与非法状态检测 */
    assert(core_gpio_init() == FW_OK);
    assert(core_gpio_read_pin(CORE_GPIO_PIN_STATUS_LED, &lvl) == FW_OK && lvl == CORE_GPIO_LEVEL_LOW);
    assert(core_gpio_toggle_pin(CORE_GPIO_PIN_STATUS_LED) == FW_OK);
    assert(core_gpio_read_pin(CORE_GPIO_PIN_STATUS_LED, &lvl) == FW_OK && lvl == CORE_GPIO_LEVEL_HIGH);

    uint32_t isr_inc = 3U;
    assert(core_gpio_register_isr(CORE_GPIO_PIN_SENSOR_TRIG, CORE_GPIO_IRQ_FALLING,
                                  test_gpio_isr_cb, &isr_inc) == FW_OK);
    s_gpio_slots[CORE_GPIO_PIN_SENSOR_TRIG].isr_cb(CORE_GPIO_PIN_SENSOR_TRIG,
                                                   s_gpio_slots[CORE_GPIO_PIN_SENSOR_TRIG].isr_arg);
    assert(s_gpio_isr_fired == 3U);

    /* 注入非法状态并验证统一 FAULT 入口与首个错误锁存 */
    s_gpio_state = (core_gpio_state_t)77;
    assert(core_gpio_write_pin(CORE_GPIO_PIN_STATUS_LED, CORE_GPIO_LEVEL_LOW) == FW_ERR_GPIO_INVALID_STATE);
    core_gpio_state_t gst = CORE_GPIO_STATE_UNINITIALIZED;
    fw_error_record_t gerr = {0};
    assert(core_gpio_get_status(&gst, &gerr) == FW_OK);
    assert(gst == CORE_GPIO_STATE_FAULT && gerr.code == FW_ERR_GPIO_INVALID_STATE);
    assert(gerr.state == 77U && gerr.line > 0U && strstr(gerr.file, "gpio_port.c") != NULL);
    /* 显式重新初始化恢复 */
    assert(core_gpio_init() == FW_OK);

    /* UART 初始化、回环收发、无新数据非终态、超时与清理错误不覆盖首错验证 */
    s_host_uart_param_err = ESP_OK;
    s_host_uart_install_err = ESP_OK;
    s_host_uart_mode_err = ESP_OK;
    s_host_uart_uninstall_err = ESP_OK;
    s_host_uart_wait_tx_err = ESP_OK;
    s_host_uart_tx_ret_override = 0;
    s_host_uart_rx_ret_override = 0;

    assert(core_uart_init(CORE_UART_ID_1, NULL) == FW_OK);
    const uint8_t frame[4] = {0x01U, 0x03U, 0x00U, 0x01U};
    assert(core_uart_tx_poll(CORE_UART_ID_1, frame, sizeof(frame), 20U) == FW_OK);

    size_t avail = 0U;
    assert(core_uart_rx_available(CORE_UART_ID_1, &avail) == FW_OK && avail == 4U);
    uint8_t rx_buf[8] = {0};
    size_t rx_len = 0U;
    assert(core_uart_rx_read(CORE_UART_ID_1, rx_buf, sizeof(rx_buf), &rx_len, 10U) == FW_OK);
    assert(rx_len == 4U && memcmp(rx_buf, frame, 4U) == 0);

    /* 无新数据超时返回 FW_OK 且 rx_len == 0，不进入 FAULT */
    assert(core_uart_rx_read(CORE_UART_ID_1, rx_buf, sizeof(rx_buf), &rx_len, 10U) == FW_OK);
    assert(rx_len == 0U);
    core_uart_state_t ust = CORE_UART_STATE_UNINITIALIZED;
    fw_error_record_t uerr = {0};
    assert(core_uart_get_status(CORE_UART_ID_1, &ust, &uerr) == FW_OK && ust == CORE_UART_STATE_READY);

    /* 验证模式设置失败且卸载驱动清理也失败时，保留首个原因并单独记录 cleanup_code */
    s_host_uart_mode_err = -200;
    s_host_uart_uninstall_err = -300;
    assert(core_uart_init(CORE_UART_ID_2, NULL) == FW_ERR_UART_CONFIG_FAILED);
    assert(core_uart_get_status(CORE_UART_ID_2, &ust, &uerr) == FW_OK);
    assert(ust == CORE_UART_STATE_FAULT);
    assert(uerr.code == FW_ERR_UART_CONFIG_FAILED && uerr.native_status == -200);
    assert(uerr.cleanup_code == FW_ERR_UART_DRIVER_FAILED);

    /* 恢复主机替身错误标志 */
    s_host_uart_mode_err = ESP_OK;
    s_host_uart_uninstall_err = ESP_OK;
    assert(core_uart_deinit(CORE_UART_ID_2) == FW_OK);
    assert(core_uart_deinit(CORE_UART_ID_1) == FW_OK);
    assert(core_gpio_deinit() == FW_OK);
    puts("PASS AD_platform/mcu: GPIO/UART FSM, loopback IO, non-fault empty read and separate cleanup_code");
}

/* ==================== 4. AA_app 组合根与错误源头传播测试 =================== */

static void test_app_composition_root(void)
{
    (void)app_deinit();
    s_host_cfg_err = ESP_OK;
    s_host_uart_param_err = ESP_OK;
    s_host_uart_install_err = ESP_OK;
    s_host_uart_mode_err = ESP_OK;
    s_host_uart_uninstall_err = ESP_OK;
    s_host_task_create_err = OSAL_OK;

    /* 注入底层 UART 安装错误，验证 AA_app 捕获本层位置并保留底层 origin 源头 */
    s_host_uart_install_err = -555;
    assert(app_init() == FW_ERR_APP_PLATFORM_INIT_FAILED);
    app_state_t ast = APP_STATE_UNINITIALIZED;
    fw_error_record_t aerr = {0};
    assert(app_get_status(&ast, &aerr) == FW_OK);
    assert(ast == APP_STATE_FAULT);
    assert(aerr.code == FW_ERR_APP_PLATFORM_INIT_FAILED);
    assert(aerr.cause_code == FW_ERR_UART_DRIVER_FAILED);
    assert(aerr.native_status_valid && aerr.native_status == -555);
    assert(strstr(aerr.file, "app_main.c") != NULL);
    assert(aerr.origin_file != NULL && strstr(aerr.origin_file, "uart_port.c") != NULL);
    assert(aerr.origin_line > 0U);

    /* 显式反初始化后正常启动组合根 */
    s_host_uart_install_err = ESP_OK;
    assert(app_deinit() == FW_OK);
    assert(app_init() == FW_OK);
    assert(app_get_status(&ast, &aerr) == FW_OK && ast == APP_STATE_RUNNING);
    assert(app_init() == FW_ERR_APP_ALREADY_INIT);
    assert(s_uart_channels[CORE_UART_ID_1].config.mode == CORE_UART_MODE_NORMAL);
    assert(s_uart_channels[CORE_UART_ID_1].config.rts_de_pin == BOARD_GPIO_PIN_NC);
    assert(s_uart_channels[CORE_UART_ID_1].config.tx_buf_bytes == 0U);
    assert(!s_uart_channels[CORE_UART_ID_2].driver_installed);
    assert(s_motor_test.state == APP_MOTOR_IDLE && s_host_loopback_len[CORE_UART_ID_1] == 0U);
    assert(app_deinit() == FW_OK);
    puts("PASS AA_app: composition root init, rollback cleanup and origin error propagation");
}

/* 真实UART适配层分片响应测试：仅替换底层硬件回环，不替换生产解析器。 */
static uint8_t s_fragment_frame[21];
static size_t s_fragment_bytes;
static size_t s_fragment_cursor;
static bool s_discard_response;

static void release_fragment(void)
{
    if (s_fragment_cursor < s_fragment_bytes)
    {
        size_t used = s_host_loopback_len[CORE_UART_ID_1];
        s_host_loopback_buf[CORE_UART_ID_1][used] = s_fragment_frame[s_fragment_cursor++];
        s_host_loopback_len[CORE_UART_ID_1] = used + 1U;
    }
}

static void simulate_motor_response(core_uart_id_t id, const uint8_t *tx, size_t bytes)
{
    (void)bytes;
    assert(id == CORE_UART_ID_1);
    s_fragment_cursor = 0U;
    s_fragment_bytes = 0U;
    if (s_discard_response)
    {
        return;
    }
    memset(s_fragment_frame, 0, sizeof(s_fragment_frame));
    if (tx[1] == 4U)
    {
        s_fragment_frame[0] = tx[0];
        s_fragment_frame[1] = 4U;
        s_fragment_frame[2] = 16U;
        s_fragment_bytes = 21U;
    }
    else
    {
        memcpy(s_fragment_frame, tx, 6U);
        s_fragment_bytes = 8U;
    }
    uint16_t crc = bsp_kamoer_crc16(s_fragment_frame, s_fragment_bytes - 2U);
    s_fragment_frame[s_fragment_bytes - 2U] = (uint8_t)crc;
    s_fragment_frame[s_fragment_bytes - 1U] = (uint8_t)(crc >> 8U);
}

static void test_motor_uart_adapter(void)
{
    core_uart_config_t config;
    assert(core_uart_get_default_config(CORE_UART_ID_1, &config) == FW_OK);
    config.mode = CORE_UART_MODE_NORMAL;
    config.rts_de_pin = BOARD_GPIO_PIN_NC;
    config.tx_buf_bytes = 0U;
    assert(core_uart_init(CORE_UART_ID_1, &config) == FW_OK);
    bsp_kamoer_t motor = {0};
    assert(bsp_adapter_port_kamoer_bind(&motor) == FW_OK);
    s_host_uart_tx_hook = simulate_motor_response;
    s_host_delay_hook = release_fragment;
    s_discard_response = false;
    bsp_kamoer_feedback_t feedback = {0};
    assert(bsp_kamoer_read_feedback(&motor, &feedback) == FW_OK);
    assert(s_fragment_cursor == 21U);
    assert(bsp_kamoer_start(&motor) == FW_OK);
    assert(s_fragment_cursor == 8U);
    s_discard_response = true;
    s_host_tick = UINT32_MAX - 10U;
    uint32_t started = s_host_tick;
    assert(bsp_kamoer_read_feedback(&motor, &feedback) == FW_ERR_KAMOER_TIMEOUT);
    assert((uint32_t)(s_host_tick - started) == MOTOR_TRANSACTION_TIMEOUT_MS);
    s_host_uart_tx_hook = NULL;
    s_host_delay_hook = NULL;
    assert(core_uart_deinit(CORE_UART_ID_1) == FW_OK);
    puts("PASS motor UART adapter: fragmented frames, auto direction, total timeout across tick wrap");
}

void test_motor_apis(void);

int main(void)
{
    test_fw_error_and_ring_buffer();
    test_osal_apis();
    test_gpio_and_uart_ports();
    test_app_composition_root();
    test_motor_apis();
    test_motor_uart_adapter();
    return 0;
}
