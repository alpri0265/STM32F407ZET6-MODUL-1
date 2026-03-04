#include "cone.h"
#include "planner.h"

void cone_start(void)
{
    motion_block_t b;
    b.axis = AXIS_X;
    b.target_mm = -1.0f;
    b.feed = 100.0f;
    planner_push(&b);
}
