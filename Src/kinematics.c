#include "kinematics.h"
#include "system_config.h"

float kinematics_mm_to_steps(axis_id_t axis, float mm)
{
    const axis_cfg_t* c = system_axis_cfg(axis);
    return mm * c->steps_per_mm;
}
