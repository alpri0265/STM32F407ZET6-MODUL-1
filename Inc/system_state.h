#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SYS_STATE_INIT = 0,
    SYS_STATE_READY,
    SYS_STATE_HOLD,
    SYS_STATE_ERROR
} system_state_t;

void system_state_init(void);
void system_state_process(void);

system_state_t system_state_get(void);
bool system_state_is(system_state_t s);

void system_request_hold(void);
void system_request_resume(void);
void system_request_error(uint16_t fault);
void system_request_reset(void);

#endif
