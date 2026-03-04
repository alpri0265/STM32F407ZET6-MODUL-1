#ifndef ENC_IF_H
#define ENC_IF_H
#include <stdbool.h>
#include <stdint.h>

void enc_if_a_irq_trigger(void);
bool enc_if_a_pending(void);
void enc_if_a_clear_pending(void);
bool enc_if_a_high(void);
bool enc_if_b_high(void);
bool enc_if_btn_raw(void);
uint32_t enc_if_get_tick_ms(void);

/* Викликати з TIM6 ISR кожні 1 мс — опитування A,B і квадратурна стейт-машина */
void enc_if_poll_1ms(void);

/* Повертає 1=CW, 2=CCW, 0=нічого; після виклику значення скидається */
int enc_if_take_encoder_step(void);

/* Ініціалізація стану енкодера (викликати з encoder_menu_init) */
void enc_if_init_encoder_state(void);

#endif
