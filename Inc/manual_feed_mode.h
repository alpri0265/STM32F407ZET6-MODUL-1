#ifndef MANUAL_FEED_MODE_H
#define MANUAL_FEED_MODE_H

#include <stdint.h>

typedef enum {
    FEED_MODE_AUTO = 0,
    FEED_MODE_MANUAL = 1
} feed_mode_t;

typedef enum {
    ENCODER_AXIS_NONE = 0,
    ENCODER_AXIS_Z    = 1,
    ENCODER_AXIS_X    = 2
} encoder_axis_t;

typedef enum {
    STEP_SCALE_0_001 = 0,
    STEP_SCALE_0_01  = 1,
    STEP_SCALE_0_1   = 2
} step_scale_t;

void manual_feed_mode_init(void);
feed_mode_t manual_feed_get_mode(void);
encoder_axis_t manual_feed_get_axis(void);
step_scale_t manual_feed_get_step_scale(void);

#endif /* MANUAL_FEED_MODE_H */

