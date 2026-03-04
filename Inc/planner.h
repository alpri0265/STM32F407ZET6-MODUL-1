#ifndef PLANNER_H
#define PLANNER_H
#include "motion_block.h"
void planner_init(void);
void planner_process(void);
void planner_push(const motion_block_t* b);
#endif
