#include "service_bubble.h"

#include "dosing_control_config.h"
#include <stddef.h>

_Static_assert(BUBBLE_CALIBRATION_SAMPLES > 0U && BUBBLE_CALIBRATION_SAMPLES <= 1000U,
               "invalid bubble sample count");
_Static_assert(BUBBLE_LIQUID_DELTA_MV <= 3600U, "invalid bubble delta");

void service_bubble_reset(service_bubble_t *self)
{
    *self = (service_bubble_t){.min_mv = UINT32_MAX};
}

fw_error_t service_bubble_add(service_bubble_t *self, uint32_t mv)
{
    if (self == NULL || mv > 3600U)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    if (self->complete)
    {
        return FW_ERR_PORT_NOT_READY;
    }
    self->sum_mv += mv;
    ++self->samples;
    if (mv < self->min_mv)
    {
        self->min_mv = mv;
    }
    if (mv > self->max_mv)
    {
        self->max_mv = mv;
    }
    if (self->samples < BUBBLE_CALIBRATION_SAMPLES)
    {
        return FW_OK;
    }
    self->complete = true;
    if (self->max_mv - self->min_mv > BUBBLE_CALIBRATION_SPREAD_MV)
    {
        return FW_ERR_BUBBLE_UNSTABLE;
    }
    self->baseline_mv = (self->sum_mv + self->samples / 2U) / self->samples;
    self->valid = true;
    return FW_OK;
}

fw_error_t service_bubble_liquid(const service_bubble_t *self, uint32_t mv, bool *present)
{
    if (self == NULL || present == NULL || mv > 3600U)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    if (!self->valid)
    {
        return FW_ERR_PORT_NOT_READY;
    }
    *present = mv > self->baseline_mv && mv - self->baseline_mv > BUBBLE_LIQUID_DELTA_MV;
    return FW_OK;
}
