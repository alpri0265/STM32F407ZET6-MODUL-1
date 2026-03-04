#include "enc_if.h"
#include "main.h"
#include <stdint.h>

static volatile bool a_pending;

/* Енкодер: опитування з TIM6 кожні 1 мс */
#define ENC_COOLDOWN_TICKS    30u  /* мс між кроками */
#define ENC_STABLE_READS      1u   /* 1 = відразу, 2+ = відсікає шум але може блокувати швидке обертання */
static const int8_t enc_cw_next[]  = { 2, 0, 3, 1 };
static const int8_t enc_ccw_next[]  = { 1, 3, 0, 2 };

static volatile uint32_t enc_tick_ms;
static volatile uint8_t enc_state;
static volatile uint8_t enc_stable_count;
static volatile uint8_t enc_last_raw;
static volatile uint32_t enc_last_step_tick;
static volatile int enc_pending;  /* 0, 1=CW, 2=CCW */

void enc_if_a_irq_trigger(void)
{
    a_pending = true;
}

bool enc_if_a_pending(void)
{
    return a_pending;
}

void enc_if_a_clear_pending(void)
{
    a_pending = false;
}

bool enc_if_a_high(void)
{
    return HAL_GPIO_ReadPin(MPG_A_GPIO_Port, MPG_A_Pin) == GPIO_PIN_SET;
}

bool enc_if_b_high(void)
{
    return HAL_GPIO_ReadPin(MPG_B_GPIO_Port, MPG_B_Pin) == GPIO_PIN_SET;
}

/* 1 = KEY активний при HIGH; 0 = KEY активний при LOW (KEY→GND) */
#define ENC_BTN_ACTIVE_HIGH  0
/* 1 = поміняти A↔B (якщо S1/S2 підключені навпаки) */
#define ENC_SWAP_AB          0

bool enc_if_btn_raw(void)
{
    bool high = (HAL_GPIO_ReadPin(MPG_BTN_GPIO_Port, MPG_BTN_Pin) == GPIO_PIN_SET);
#if ENC_BTN_ACTIVE_HIGH
    return high;   /* натиснуто = HIGH */
#else
    return !high;  /* натиснуто = LOW (KEY→GND) */
#endif
}

uint32_t enc_if_get_tick_ms(void)
{
    return HAL_GetTick();
}

void enc_if_poll_1ms(void)
{
    enc_tick_ms++;
    bool a = enc_if_a_high(), b = enc_if_b_high();
#if ENC_SWAP_AB
    uint8_t raw = (b ? 2u : 0u) | (a ? 1u : 0u);  /* A↔B */
#else
    uint8_t raw = (a ? 2u : 0u) | (b ? 1u : 0u);
#endif
    if (raw == enc_last_raw) {
        if (enc_stable_count < 255u) enc_stable_count++;
    } else {
        enc_last_raw = raw;
        enc_stable_count = 0;
    }
    uint8_t new_s = (enc_stable_count >= ENC_STABLE_READS) ? raw : enc_state;
    uint32_t since = enc_tick_ms - enc_last_step_tick;
    if (since > 0x80000000u) since = 0;
    if (new_s != enc_state && since >= ENC_COOLDOWN_TICKS && enc_pending == 0) {
        if (enc_cw_next[enc_state] == (int8_t)new_s) {
            enc_pending = 1;
            enc_last_step_tick = enc_tick_ms;
        } else if (enc_ccw_next[enc_state] == (int8_t)new_s) {
            enc_pending = 2;
            enc_last_step_tick = enc_tick_ms;
        }
        enc_state = new_s;
    } else if (new_s != enc_state) {
        enc_state = new_s;
    }
}

int enc_if_take_encoder_step(void)
{
    /* Коротка критична секція, щоб ISR не втратив крок між читанням і скиданням */
    __disable_irq();
    int v = enc_pending;
    enc_pending = 0;
    __enable_irq();
    return v;
}

void enc_if_init_encoder_state(void)
{
    enc_tick_ms = 0;
    enc_last_step_tick = 0;
    enc_pending = 0;
    {
        bool a = enc_if_a_high(), b = enc_if_b_high();
#if ENC_SWAP_AB
        enc_last_raw = (b ? 2u : 0u) | (a ? 1u : 0u);
#else
        enc_last_raw = (a ? 2u : 0u) | (b ? 1u : 0u);
#endif
    }
    enc_stable_count = ENC_STABLE_READS;
    enc_state = enc_last_raw;
}
