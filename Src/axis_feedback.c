#include "axis_feedback.h"
#include "main.h"
#include <stdint.h>

/* Позиція в "ум" (1 cnt = 1 um = 0.001 мм) */
static volatile int32_t pos_um[2];
static uint32_t last_cnt2;
static uint16_t last_cnt3;

void axis_feedback_init(void)
{
    pos_um[AXIS_X] = 0;
    pos_um[AXIS_Z] = 0;
    last_cnt2 = (uint32_t)TIM2->CNT;
    last_cnt3 = (uint16_t)TIM3->CNT;
}

void axis_feedback_tick_1ms(void)
{
    /* X: TIM2 (32-bit) */
    uint32_t c2 = (uint32_t)TIM2->CNT;
    int32_t d2 = (int32_t)(c2 - last_cnt2);
    last_cnt2 = c2;
    pos_um[AXIS_X] += d2;

    /* Z: TIM3 (16-bit) */
    uint16_t c3 = (uint16_t)TIM3->CNT;
    int16_t d3 = (int16_t)(c3 - last_cnt3);
    last_cnt3 = c3;
    pos_um[AXIS_Z] += (int32_t)d3;
}

float axis_feedback_pos_mm(axis_id_t axis)
{
    /* 0.001 mm resolution */
    return (float)pos_um[axis] * 0.001f;
}

int32_t axis_feedback_pos_um(axis_id_t axis)
{
    return pos_um[axis];
}

void axis_feedback_zero(axis_id_t axis)
{
    /* tick_1ms() runs in TIM6 ISR; sync counters atomically */
    __disable_irq();
    if (axis == AXIS_X) {
        pos_um[AXIS_X] = 0;
        last_cnt2 = (uint32_t)TIM2->CNT;
    } else if (axis == AXIS_Z) {
        pos_um[AXIS_Z] = 0;
        last_cnt3 = (uint16_t)TIM3->CNT;
    }
    __enable_irq();
}
