#include "system_config.h"

static axis_cfg_t cfg[2];

void system_config_init(void)
{
    cfg[AXIS_X] = (axis_cfg_t){400.0f, 3000.0f, -200.0f, 0.0f};
    cfg[AXIS_Z] = (axis_cfg_t){400.0f, 2500.0f, -500.0f, 0.0f};
}

const axis_cfg_t* system_axis_cfg(axis_id_t a)
{
    return &cfg[a];
}
