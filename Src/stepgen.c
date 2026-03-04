#include "stepgen.h"
#include "tim_if.h"
#include <stdint.h>

static volatile bool busy;

void stepgen_init(void)
{
    busy = false;
}

void stepgen_move(axis_id_t axis, float mm, float feed)
{
    (void)axis;
    (void)mm;

    uint32_t freq = (uint32_t)(feed * 10.0f);
    tim_if_start(freq);

    busy = true;
}

void stepgen_stop(void)
{
    tim_if_stop();
    busy = false;
}

bool stepgen_busy(void)
{
    return busy;
}
