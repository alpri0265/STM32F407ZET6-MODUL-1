#ifndef STEPGEN_H
#define STEPGEN_H

#include <stdbool.h>
#include <stdint.h>
#include "system_config.h"

void stepgen_init(void);
void stepgen_move(axis_id_t axis, float mm, float feed);
void stepgen_stop(void);
bool stepgen_busy(void);

#endif /* STEPGEN_H */
