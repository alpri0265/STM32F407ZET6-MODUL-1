#include "axis_feedback.h"
#include <stdint.h>
/* TIM encoder mode counters updated in BSP */
static volatile int32_t enc_cnt[2];

void axis_feedback_init(void)
{
    enc_cnt[AXIS_X] = 0;
    enc_cnt[AXIS_Z] = 0;
}

float axis_feedback_pos_mm(axis_id_t axis)
{
    /* 0.001 mm resolution */
    return (float)enc_cnt[axis] * 0.001f;
}
