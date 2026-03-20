#ifndef AXIS_FEEDBACK_H
#define AXIS_FEEDBACK_H

#include <stdint.h>
#include "system_config.h"

void axis_feedback_init(void);
void axis_feedback_tick_1ms(void);
float axis_feedback_pos_mm(axis_id_t axis);
int32_t axis_feedback_pos_um(axis_id_t axis); /* 1 um = 0.001 mm */
/* Обнулення: запам’ятовує опорний TIM->CNT у Flash (сектор 7 @ 0x08060000). */
void axis_feedback_zero(axis_id_t axis);

/* Зберігає в Flash останнє значення, яке зараз відображається для осей (Xf/Zf). */
void axis_feedback_save_last_displayed(void);

/* Скільки мс минуло від останнього руху лінійних енкодерів. */
uint32_t axis_feedback_ms_since_last_move(void);

#endif /* AXIS_FEEDBACK_H */
