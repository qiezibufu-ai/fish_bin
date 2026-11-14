#include "service_dosing.h"

#include <assert.h>
#include <stdio.h>

typedef struct
{
    uint32_t now;
    uint32_t count;
    unsigned calls;
    unsigned starts;
    unsigned stops;
    bool bad_count;
    bool bad_start;
    bool bad_stop;
} fixture_t;

static uint32_t clock_ms(void *context)
{
    return ((fixture_t *)context)->now;
}

static fw_error_t read_count(void *context, bsp_dosing_inputs_snapshot_t *snapshot)
{
    fixture_t *f = context;
    if (f->bad_count)
    {
        return FW_ERR_PCNT_READ;
    }
    *snapshot = (bsp_dosing_inputs_snapshot_t){
        .drop_count = f->count, .count_valid = true, .timestamp_ms = f->now
    };
    return FW_OK;
}

static fw_error_t pump(void *context, service_dosing_pump_op_t op, uint32_t value,
                       fw_error_record_t *error)
{
    fixture_t *f = context;
    ++f->calls;
    if (op == SERVICE_DOSING_PUMP_START)
    {
        ++f->starts;
        /* 在START等待应答期间到达的第一滴也属于本轮。 */
        ++f->count;
        if (f->bad_start)
        {
            fw_error_capture_at(error, FW_ERR_KAMOER_EXECUTION_UNKNOWN, FW_ERR_UART_TIMEOUT,
                                1U, 1U, 1U, f->now, "mock_uart", "start", 42U);
            return error->code;
        }
    }
    else if (op == SERVICE_DOSING_PUMP_STOP)
    {
        ++f->stops;
        if (f->bad_stop)
        {
            return FW_ERR_KAMOER_EXECUTION_UNKNOWN;
        }
    }
    else if (op == SERVICE_DOSING_PUMP_SPEED || op == SERVICE_DOSING_PUMP_START_SPEED)
    {
        assert(value == 2000U);
    }
    return FW_OK;
}

static const service_dosing_request_t s_request = {
    .target_drops = 3U, .speed_centi_rpm = 2000U, .no_drop_ms = 100U,
    .max_run_ms = 500U, .tail_quiet_ms = 50U, .tail_max_ms = 150U
};

static void bind(service_dosing_t *service, fixture_t *f)
{
    *f = (fixture_t){.count = 100U, .now = UINT32_MAX - 20U};
    const service_dosing_ops_t ops = {
        .inputs = {.read_snapshot = read_count, .context = f},
        .pump = pump, .pump_context = f, .now_ms = clock_ms, .clock_context = f
    };
    assert(service_dosing_init(service, &ops) == FW_OK);
    assert(service_dosing_start(service, &s_request) == FW_OK);
    assert(service_dosing_start(service, &s_request) == FW_ERR_DOSING_BUSY);
    assert(f->calls == 0U);
}

static void setup(service_dosing_t *service, fixture_t *f)
{
    bind(service, f);
    for (unsigned i = 0U; i < 5U; ++i)
    {
        assert(service_dosing_step(service) == FW_OK);
    }
    assert(service->result.state == SERVICE_DOSING_START && f->starts == 0U);
}

static void run(service_dosing_t *service, fixture_t *f)
{
    setup(service, f);
    assert(service_dosing_step(service) == FW_OK);
    assert(service_dosing_step(service) == FW_OK);
    assert(service->result.drops == 1U && f->starts == 1U);
}

void test_service_dosing(void)
{
    service_dosing_t service;
    fixture_t f;
    service_dosing_result_t result;

    run(&service, &f);
    f.count += 2U;
    assert(service_dosing_step(&service) == FW_OK);
    assert(service.result.state == SERVICE_DOSING_TAIL && service.result.stop_drops == 3U);
    f.now += 30U; /* 单调时钟回绕。 */
    ++f.count;
    assert(service_dosing_step(&service) == FW_OK);
    f.now += 49U;
    assert(service_dosing_step(&service) == FW_OK && service.result.state == SERVICE_DOSING_TAIL);
    ++f.now;
    assert(service_dosing_step(&service) == FW_OK);
    assert(service_dosing_get_result(&service, &result) == FW_OK);
    assert(result.state == SERVICE_DOSING_DONE && result.drops == 4U && result.stop_acked);
    assert(f.stops == 2U);
    assert(service_dosing_step(&service) == FW_OK && f.stops == 2U);
    assert(service_dosing_start(&service, &s_request) == FW_OK);
    assert(service.result.request_id == result.request_id + 1U && !service.result.count_valid);
    assert(service_dosing_cancel(&service) == FW_OK && f.starts == 1U && f.stops == 2U);

    run(&service, &f);
    f.now += 100U;
    assert(service_dosing_step(&service) == FW_ERR_DOSING_NO_DROP);
    assert(service.result.stop_acked && f.stops == 2U);
    assert(service_dosing_step(&service) == FW_ERR_DOSING_NO_DROP && f.stops == 2U);

    setup(&service, &f);
    f.bad_count = true;
    assert(service_dosing_step(&service) == FW_ERR_DOSING_COUNT);
    assert(f.starts == 0U && f.stops == 1U);

    setup(&service, &f);
    f.bad_start = true;
    f.bad_stop = true;
    assert(service_dosing_step(&service) == FW_ERR_DOSING_START);
    assert(f.starts == 1U && f.stops == 2U && !service.result.stop_acked);
    assert(service.result.error.cause_code == FW_ERR_KAMOER_EXECUTION_UNKNOWN);
    assert(service.result.error.origin_line == 42U);
    assert(service.result.error.cleanup_code == FW_ERR_KAMOER_EXECUTION_UNKNOWN);
    assert(service_dosing_step(&service) == FW_ERR_DOSING_START && f.calls == 7U);

    run(&service, &f);
    f.count += 2U;
    f.bad_stop = true;
    assert(service_dosing_step(&service) == FW_ERR_DOSING_STOP);
    assert(!service.result.stop_acked && f.stops == 2U);
    assert(service_dosing_cancel(&service) == FW_ERR_DOSING_STOP && f.stops == 2U);

    run(&service, &f);
    f.count = 99U;
    assert(service_dosing_step(&service) == FW_ERR_DOSING_COUNT);
    assert(!service.result.count_valid && service.result.stop_acked && f.stops == 2U);

    run(&service, &f);
    f.now += 500U;
    assert(service_dosing_step(&service) == FW_ERR_DOSING_RUN_TIMEOUT);
    assert(f.stops == 2U);

    run(&service, &f);
    assert(service_dosing_cancel(&service) == FW_OK);
    f.now += 50U;
    assert(service_dosing_step(&service) == FW_OK);
    assert(service.result.state == SERVICE_DOSING_CANCELLED && f.stops == 2U);

    run(&service, &f);
    f.count += 2U;
    assert(service_dosing_step(&service) == FW_OK);
    f.now += 150U;
    ++f.count;
    assert(service_dosing_step(&service) == FW_ERR_DOSING_TAIL_TIMEOUT);
    assert(service.result.stop_acked && f.stops == 2U);

    run(&service, &f);
    service.result.state = (service_dosing_state_t)99;
    assert(service_dosing_step(&service) == FW_ERR_DOSING_STATE);
    assert(service.result.error.state == 99U && f.stops == 2U);
    puts("PASS dosing: baseline, tail, cancellation, time wrap, timeouts, unknown START and failed STOP");
}
