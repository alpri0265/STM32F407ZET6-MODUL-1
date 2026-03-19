#ifndef AXIS_FEEDBACK_H
#define AXIS_FEEDBACK_H

#include <stdint.h>
#include "system_config.h"

void axis_feedback_init(void);
void axis_feedback_tick_1ms(void);
float axis_feedback_pos_mm(axis_id_t axis);
int32_t axis_feedback_pos_um(axis_id_t axis); /* 1 um = 0.001 mm */

#endif /* AXIS_FEEDBACK_H */
