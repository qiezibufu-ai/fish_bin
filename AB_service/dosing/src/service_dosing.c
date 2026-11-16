#include "service_dosing.h"

#include <stddef.h>

static uint32_t dosing_now(const service_dosing_t *self)
{
    return self->ops.now_ms(self->ops.clock_context);
}

static fw_error_t dosing_command(service_dosing_t *self, service_dosing_pump_op_t op,
                                 uint32_t value, fw_error_record_t *origin)
{
    *origin = (fw_error_record_t){0};
    return self->ops.pump(self->ops.pump_context, op, value, origin);
}

static fw_error_t service_dosing_fail_at(service_dosing_t *self, fw_error_t code,
                                         fw_error_t cause, const fw_error_record_t *origin,
                                         bool cleanup, const char *file,
                                         const char *function, uint32_t line)
{
    fw_error_record_t *error = &self->result.error;
    fw_error_capture_at(error, code, cause, self->result.state, (uint16_t)self->result.state,
                        self->result.request_id, dosing_now(self), file, function, line);
    if (origin != NULL && origin->code != FW_OK)
    {
        fw_error_set_origin(error, origin);
        error->cleanup_code = origin->cleanup_code;
    }
    self->result.state = SERVICE_DOSING_FAULT;
    if (cleanup && self->motion_possible)
    {
        fw_error_record_t stop_origin;
        self->result.stop_drops = self->result.drops;
        fw_error_t stop = dosing_command(self, SERVICE_DOSING_PUMP_STOP, 0U, &stop_origin);
        self->result.stop_acked = stop == FW_OK;
        self->motion_possible = stop != FW_OK;
        if (stop != FW_OK)
        {
            error->cleanup_code = stop;
        }
    }
    return code;
}

#define DOSING_FAIL(self, code, cause, origin, cleanup) \
    service_dosing_fail_at((self), (code), (cause), (origin), (cleanup), \
                           __FILE__, __func__, (uint32_t)__LINE__)

static fw_error_t dosing_sample(service_dosing_t *self, bool baseline)
{
    bsp_dosing_inputs_snapshot_t sample = {0};
    fw_error_t err = self->ops.inputs.read_snapshot(self->ops.inputs.context, &sample);
    if (err != FW_OK || !sample.count_valid ||
        (!baseline && (sample.drop_count < self->baseline ||
                       sample.drop_count - self->baseline < self->result.drops)))
    {
        fw_error_record_t origin = {0};
        if (err != FW_OK && self->ops.input_error != NULL)
        {
            self->ops.input_error(self->ops.inputs.context, &origin);
        }
        self->result.count_valid = false;
        return DOSING_FAIL(self, FW_ERR_DOSING_COUNT, err, &origin, true);
    }
    if (baseline)
    {
        self->baseline = sample.drop_count;
    }
    uint32_t drops = sample.drop_count - self->baseline;
    if (baseline || drops != self->result.drops)
    {
        self->last_drop_ms = dosing_now(self);
    }
    self->result.drops = drops;
    self->result.count_valid = true;
    return FW_OK;
}

static fw_error_t dosing_stop(service_dosing_t *self)
{
    fw_error_record_t origin;
    self->result.stop_drops = self->result.drops;
    fw_error_t err = dosing_command(self, SERVICE_DOSING_PUMP_STOP, 0U, &origin);
    self->result.stop_acked = err == FW_OK;
    if (err != FW_OK)
    {
        return DOSING_FAIL(self, FW_ERR_DOSING_STOP, err, &origin, false);
    }
    self->motion_possible = false;
    self->stopped_ms = dosing_now(self);
    self->last_drop_ms = self->stopped_ms;
    self->result.state = SERVICE_DOSING_TAIL;
    return FW_OK;
}

fw_error_t service_dosing_init(service_dosing_t *self, const service_dosing_ops_t *ops)
{
    if (self == NULL || ops == NULL || ops->inputs.read_snapshot == NULL ||
        ops->pump == NULL || ops->now_ms == NULL)
    {
        return FW_ERR_DOSING_ARG;
    }
    *self = (service_dosing_t){.ops = *ops, .bound = true};
    return FW_OK;
}

fw_error_t service_dosing_start(service_dosing_t *self, const service_dosing_request_t *request)
{
    if (self == NULL || request == NULL || request->target_drops == 0U ||
        request->target_drops > 10000U || request->speed_centi_rpm < 100U ||
        request->speed_centi_rpm > 50000U || request->no_drop_ms == 0U ||
        request->max_run_ms > 60000U || request->no_drop_ms > request->max_run_ms ||
        request->tail_quiet_ms == 0U || request->tail_quiet_ms > request->tail_max_ms ||
        request->tail_max_ms > 10000U)
    {
        return FW_ERR_DOSING_ARG;
    }
    if (!self->bound)
    {
        return FW_ERR_DOSING_NOT_READY;
    }
    switch (self->result.state)
    {
        case SERVICE_DOSING_IDLE:
        case SERVICE_DOSING_DONE:
        case SERVICE_DOSING_CANCELLED:
            break;
        case SERVICE_DOSING_FAULT:
            return self->result.error.code;
        case SERVICE_DOSING_PRESTOP:
        case SERVICE_DOSING_DIRECTION:
        case SERVICE_DOSING_MODE:
        case SERVICE_DOSING_START_SPEED:
        case SERVICE_DOSING_SPEED:
        case SERVICE_DOSING_START:
        case SERVICE_DOSING_RUNNING:
        case SERVICE_DOSING_TAIL:
        case SERVICE_DOSING_RETRACT_DIRECTION:
        case SERVICE_DOSING_RETRACT_START:
        case SERVICE_DOSING_RETRACT_RUNNING:
            return FW_ERR_DOSING_BUSY;
        default:
            return DOSING_FAIL(self, FW_ERR_DOSING_STATE, FW_OK, NULL, true);
    }
    uint32_t id = self->result.request_id + 1U;
    self->request = *request;
    self->result = (service_dosing_result_t){
        .state = SERVICE_DOSING_PRESTOP,
        .request_id = id == 0U ? 1U : id,
        .target_drops = request->target_drops
    };
    self->cancelled = false;
    return FW_OK;
}

fw_error_t service_dosing_step(service_dosing_t *self)
{
    if (self == NULL)
    {
        return FW_ERR_DOSING_ARG;
    }
    if (!self->bound)
    {
        return FW_ERR_DOSING_NOT_READY;
    }
    service_dosing_pump_op_t op;
    service_dosing_state_t next;
    uint32_t value = 0U;
    uint32_t now;
    fw_error_t err;
    switch (self->result.state)
    {
        case SERVICE_DOSING_IDLE:
        case SERVICE_DOSING_DONE:
        case SERVICE_DOSING_CANCELLED:
            return FW_OK;
        case SERVICE_DOSING_FAULT:
            return self->result.error.code;
        case SERVICE_DOSING_PRESTOP:
            op = SERVICE_DOSING_PUMP_STOP;
            next = SERVICE_DOSING_DIRECTION;
            break;
        case SERVICE_DOSING_DIRECTION:
            op = SERVICE_DOSING_PUMP_DIRECTION;
            value = self->request.reverse ? 1U : 0U;
            next = SERVICE_DOSING_MODE;
            break;
        case SERVICE_DOSING_MODE:
            op = SERVICE_DOSING_PUMP_SPEED_MODE;
            next = SERVICE_DOSING_START_SPEED;
            break;
        case SERVICE_DOSING_START_SPEED:
            op = SERVICE_DOSING_PUMP_START_SPEED;
            value = self->request.speed_centi_rpm;
            next = SERVICE_DOSING_SPEED;
            break;
        case SERVICE_DOSING_SPEED:
            op = SERVICE_DOSING_PUMP_SPEED;
            value = self->request.speed_centi_rpm;
            next = SERVICE_DOSING_START;
            break;
        case SERVICE_DOSING_START:
            err = dosing_sample(self, true);
            if (err != FW_OK)
            {
                return err;
            }
            self->started_ms = dosing_now(self);
            self->last_drop_ms = self->started_ms;
            self->motion_possible = true;
            self->result.stop_acked = false;
            op = SERVICE_DOSING_PUMP_START;
            next = SERVICE_DOSING_RUNNING;
            break;
        case SERVICE_DOSING_RUNNING:
            err = dosing_sample(self, false);
            if (err != FW_OK)
            {
                return err;
            }
            now = dosing_now(self);
            if (now - self->started_ms >= self->request.max_run_ms)
            {
                return DOSING_FAIL(self, FW_ERR_DOSING_RUN_TIMEOUT, FW_OK, NULL, true);
            }
            if (self->result.drops >= self->request.target_drops)
            {
                return dosing_stop(self);
            }
            if (now - self->last_drop_ms >= self->request.no_drop_ms)
            {
                return DOSING_FAIL(self, FW_ERR_DOSING_NO_DROP, FW_OK, NULL, true);
            }
            return FW_OK;
        case SERVICE_DOSING_TAIL:
            err = dosing_sample(self, false);
            if (err != FW_OK)
            {
                return err;
            }
            now = dosing_now(self);
            if (now - self->last_drop_ms >= self->request.tail_quiet_ms)
            {
                self->result.state = self->cancelled ? SERVICE_DOSING_CANCELLED
                                                     : SERVICE_DOSING_RETRACT_DIRECTION;
                return FW_OK;
            }
            if (now - self->stopped_ms >= self->request.tail_max_ms)
            {
                return DOSING_FAIL(self, FW_ERR_DOSING_TAIL_TIMEOUT, FW_OK, NULL, false);
            }
            return FW_OK;
        case SERVICE_DOSING_RETRACT_DIRECTION:
            op = SERVICE_DOSING_PUMP_DIRECTION;
            value = self->request.reverse ? 0U : 1U;
            next = SERVICE_DOSING_RETRACT_START;
            break;
        case SERVICE_DOSING_RETRACT_START:
            op = SERVICE_DOSING_PUMP_START;
            self->stopped_ms = dosing_now(self);
            self->motion_possible = true;
            self->result.stop_acked = false;
            next = SERVICE_DOSING_RETRACT_RUNNING;
            break;
        case SERVICE_DOSING_RETRACT_RUNNING:
            now = dosing_now(self);
            if (now - self->stopped_ms >= 500U) {
                fw_error_record_t error;
                err = dosing_command(self, SERVICE_DOSING_PUMP_STOP, 0U, &error);
                self->result.stop_acked = err == FW_OK;
                if (err != FW_OK) {
                    return DOSING_FAIL(self, FW_ERR_DOSING_STOP, err, &error, false);
                }
                self->motion_possible = false;
                self->result.state = SERVICE_DOSING_DONE;
            }
            return FW_OK;
        default:
            return DOSING_FAIL(self, FW_ERR_DOSING_STATE, FW_OK, NULL, true);
    }
    fw_error_record_t origin;
    err = dosing_command(self, op, value, &origin);
    if (err != FW_OK)
    {
        fw_error_t code = op == SERVICE_DOSING_PUMP_START ? FW_ERR_DOSING_START :
                          op == SERVICE_DOSING_PUMP_STOP ? FW_ERR_DOSING_STOP :
                                                         FW_ERR_DOSING_SETUP;
        return DOSING_FAIL(self, code, err, &origin, true);
    }
    if (op == SERVICE_DOSING_PUMP_STOP)
    {
        self->result.stop_acked = true;
    }
    self->result.state = next;
    return FW_OK;
}

fw_error_t service_dosing_cancel(service_dosing_t *self)
{
    if (self == NULL)
    {
        return FW_ERR_DOSING_ARG;
    }
    if (!self->bound)
    {
        return FW_ERR_DOSING_NOT_READY;
    }
    switch (self->result.state)
    {
        case SERVICE_DOSING_IDLE:
        case SERVICE_DOSING_DONE:
        case SERVICE_DOSING_CANCELLED:
            return FW_OK;
        case SERVICE_DOSING_FAULT:
            return self->result.error.code;
        case SERVICE_DOSING_TAIL:
            self->cancelled = true;
            return FW_OK;
        case SERVICE_DOSING_RUNNING:
        case SERVICE_DOSING_RETRACT_RUNNING:
            self->cancelled = true;
            return dosing_stop(self);
        case SERVICE_DOSING_PRESTOP:
        case SERVICE_DOSING_DIRECTION:
        case SERVICE_DOSING_MODE:
        case SERVICE_DOSING_START_SPEED:
        case SERVICE_DOSING_SPEED:
        case SERVICE_DOSING_START:
        case SERVICE_DOSING_RETRACT_DIRECTION:
        case SERVICE_DOSING_RETRACT_START:
            self->result.state = SERVICE_DOSING_CANCELLED;
            return FW_OK;
        default:
            return DOSING_FAIL(self, FW_ERR_DOSING_STATE, FW_OK, NULL, true);
    }
}

fw_error_t service_dosing_get_result(const service_dosing_t *self, service_dosing_result_t *result)
{
    if (self == NULL || result == NULL)
    {
        return FW_ERR_DOSING_ARG;
    }
    if (!self->bound)
    {
        return FW_ERR_DOSING_NOT_READY;
    }
    *result = self->result;
    return FW_OK;
}
