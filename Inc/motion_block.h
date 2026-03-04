#ifndef MOTION_BLOCK_H
#define MOTION_BLOCK_H
#include "system_config.h"

typedef struct {
    axis_id_t axis;
    float target_mm;
    float feed;
} motion_block_t;

#endif
