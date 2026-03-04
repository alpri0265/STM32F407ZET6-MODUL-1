#include "buttons.h"
#include "gpio_if.h"

static bool estop_f;
static bool hold_f;
static bool resume_f;
static bool reset_f;

void buttons_init(void)
{
    estop_f = hold_f = resume_f = reset_f = false;
}

void buttons_process(void)
{
    estop_f = gpio_if_estop();
    hold_f = false;
    resume_f = false;
    reset_f = false;
}

bool buttons_estop(void)  { return estop_f; }
bool buttons_hold(void)   { return hold_f; }
bool buttons_resume(void) { return resume_f; }
bool buttons_reset(void)  { return reset_f; }
