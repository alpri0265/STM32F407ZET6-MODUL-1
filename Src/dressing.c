#include "dressing.h"
#include "planner.h"

void dressing_start(void)
{
    motion_block_t b;
    b.axis = AXIS_Z;
    b.target_mm = -5.0f;
    b.feed = 300.0f;
    planner_push(&b);
}
