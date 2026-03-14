#ifndef JOG_H
#define JOG_H
#include <stdint.h>
/* Jog осей X/Z по джойстику (JOY_UP/DOWN/LEFT/RIGHT). Викликати jog_process() з циклу або з TIM6. */
void jog_init(void);
void jog_process(void);
/* Стан джойстика для відображення: 1 = натиснуто. up/down/left/right можуть бути NULL. */
void jog_get_joy_state(unsigned int *up, unsigned int *down, unsigned int *left, unsigned int *right);
/* Лічильники кроків (діагностика): збільшуються при кожному pulse_x_step / pulse_z_step. */
void jog_get_step_counts(uint32_t *x, uint32_t *z);
/* Стан кнопки пришвидшення: 1 = натиснуто (швидший рух). */
void jog_get_rapid_state(unsigned int *rapid);
#endif
