#include "limits.h"
#include "system_config.h"

bool limits_check(axis_id_t a, float mm)
{
    const axis_cfg_t* c = system_axis_cfg(a);
    return (mm >= c->min_mm && mm <= c->max_mm);
}
