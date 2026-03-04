#ifndef BACKLASH_H
#define BACKLASH_H
#include "system_config.h"

void backlash_reset(axis_id_t axis);
float backlash_compensate(axis_id_t axis, float target_mm);

#endif
