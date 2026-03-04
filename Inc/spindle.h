#ifndef SPINDLE_H
#define SPINDLE_H
#include <stdint.h>

void spindle_init(void);
void spindle_set_rpm(uint16_t rpm);
void spindle_stop(void);

#endif
