#include "mpg.h"
#include "encoder_mpg.h"
#include "planner.h"
#include "scale_select.h"
#include "system_state.h"

void mpg_process(void)
{
    if (!system_state_is(SYS_STATE_READY)) return;

    int32_t d = encoder_mpg_delta();
    if (d == 0) return;

    motion_block_t b;
    b.axis = AXIS_X;
    b.target_mm = (float)d * scale_select_get();
    b.feed = 200.0f;

    planner_push(&b);
}
