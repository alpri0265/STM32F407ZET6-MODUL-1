#include "backlash.h"

static float last_target[2];

void backlash_reset(axis_id_t axis)
{
    last_target[axis] = 0.0f;
}

float backlash_compensate(axis_id_t axis, float target_mm)
{
    float corrected = target_mm;
    if ((target_mm - last_target[axis]) > 0.0f) {
        corrected += 0.01f; /* backlash value */
    }
    last_target[axis] = target_mm;
    return corrected;
}
