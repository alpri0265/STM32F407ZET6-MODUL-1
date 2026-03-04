#ifndef BUTTONS_H
#define BUTTONS_H
#include <stdbool.h>

void buttons_init(void);
void buttons_process(void);

bool buttons_estop(void);
bool buttons_hold(void);
bool buttons_resume(void);
bool buttons_reset(void);

#endif
