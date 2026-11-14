#include "bsp_adapter_port_drop_count.h"

#include "osal_task.h"
#include "pcnt_port.h"

#include <stddef.h>

fw_error_t bsp_adapter_port_drop_count_read(void *context, bsp_dosing_inputs_snapshot_t *snapshot)
{
    (void)context;
    if (snapshot == NULL)
    {
        return FW_ERR_PORT_INVALID_ARG;
    }
    uint32_t count;
    fw_error_t err = core_pcnt_read(&count);
    if (err != FW_OK)
    {
        return err;
    }
    *snapshot = (bsp_dosing_inputs_snapshot_t){
        .liquid = BSP_DOSING_LIQUID_UNKNOWN,
        .liquid_valid = false,
        .drop_count = count,
        .count_valid = true,
        .timestamp_ms = osal_task_now_ms()
    };
    return FW_OK;
}
