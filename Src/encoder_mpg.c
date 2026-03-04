#include "encoder_mpg.h"

static volatile int32_t delta;

void encoder_mpg_init(void)
{
    delta = 0;
}

void encoder_mpg_process(void)
{
    /* ISR updates delta */
}

int32_t encoder_mpg_delta(void)
{
    int32_t d = delta;
    delta = 0;
    return d;
}
