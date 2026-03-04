#include "axis_mode_switch.h"

static axis_id_t current = AXIS_X;

axis_id_t axis_mode_get(void)
{
    return current;
}
