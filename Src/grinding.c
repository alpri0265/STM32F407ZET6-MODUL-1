#include "grinding.h"
#include "planner.h"

void grinding_start(void)
{
    motion_block_t b;
    b.axis = AXIS_X;
    b.target_mm = -2.0f;
    b.feed = 200.0f;
    planner_push(&b);
}
