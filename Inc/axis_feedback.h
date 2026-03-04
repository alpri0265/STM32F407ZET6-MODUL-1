#ifndef AXIS_FEEDBACK_H
#define AXIS_FEEDBACK_H

#include <stdint.h>
#include "system_config.h"

void axis_feedback_init(void);
float axis_feedback_pos_mm(axis_id_t axis);

#endif /* AXIS_FEEDBACK_H */
