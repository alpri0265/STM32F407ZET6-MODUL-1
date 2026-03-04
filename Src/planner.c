#include "planner.h"
#include "stepgen.h"
#include "system_state.h"

static motion_block_t blk;
static int has_blk;

void planner_init(void)
{
    has_blk = 0;
    stepgen_init();
}

void planner_push(const motion_block_t* b)
{
    if (!has_blk) {
        blk = *b;
        has_blk = 1;
    }
}

void planner_process(void)
{
    if (!has_blk) return;
    if (!system_state_is(SYS_STATE_READY)) return;
    if (stepgen_busy()) return;

    stepgen_move(blk.axis, blk.target_mm, blk.feed);
    has_blk = 0;
}
