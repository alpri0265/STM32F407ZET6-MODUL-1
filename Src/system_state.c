#include "system_state.h"
#include "bringup_config.h"

static volatile system_state_t cur;
static volatile system_state_t req;
static volatile uint16_t fault_latch;

void system_state_init(void)
{
    cur = SYS_STATE_INIT;
    req = SYS_STATE_INIT;
    fault_latch = 0;
#if BRINGUP_MODE
    req = SYS_STATE_READY;
#endif
}

void system_state_process(void)
{
    if (cur == req) return;

    switch (cur) {
        case SYS_STATE_INIT:
            if (req == SYS_STATE_READY || req == SYS_STATE_ERROR) cur = req;
            break;
        case SYS_STATE_READY:
            if (req == SYS_STATE_HOLD || req == SYS_STATE_ERROR) cur = req;
            break;
        case SYS_STATE_HOLD:
            if (req == SYS_STATE_READY || req == SYS_STATE_ERROR) cur = req;
            break;
        case SYS_STATE_ERROR:
            if (req == SYS_STATE_INIT) cur = SYS_STATE_INIT;
            break;
        default:
            cur = SYS_STATE_ERROR;
            break;
    }
}

system_state_t system_state_get(void) { return cur; }
bool system_state_is(system_state_t s) { return cur == s; }

void system_request_hold(void)
{
#if !BRINGUP_MODE
    if (cur == SYS_STATE_READY) req = SYS_STATE_HOLD;
#endif
}

void system_request_resume(void)
{
#if !BRINGUP_MODE
    if (cur == SYS_STATE_HOLD) req = SYS_STATE_READY;
#endif
}

void system_request_error(uint16_t f)
{
    fault_latch = f;
    req = SYS_STATE_ERROR;
}

void system_request_reset(void)
{
    if (cur == SYS_STATE_ERROR) req = SYS_STATE_INIT;
}
