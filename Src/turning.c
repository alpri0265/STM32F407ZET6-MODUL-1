#include "turning.h"
#include "planner.h"

void turning_start(void)
{
    motion_block_t b;
    b.axis = AXIS_Z;
    b.target_mm = -10.0f;
    b.feed = 500.0f;
    planner_push(&b);
}
