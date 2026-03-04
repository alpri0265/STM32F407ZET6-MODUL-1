#include "encoder_menu.h"
#include "enc_if.h"
#include <stddef.h>
#include <stdint.h>

#define DEBOUNCE_MS    30
#define DEBOUNCE_TICKS (DEBOUNCE_MS)

static encoder_menu_action_t action;
static bool btn_pressed_prev;
static uint32_t btn_stable_ticks;
static bool btn_pressed_stable;
static bool prev_raw;

void encoder_menu_init(void)
{
    action = ENCODER_MENU_ACTION_NONE;
    btn_pressed_prev = false;
    btn_stable_ticks = 0;
    btn_pressed_stable = false;
    prev_raw = enc_if_btn_raw();
    enc_if_init_encoder_state();
    enc_if_a_clear_pending();
}

static void poll_button(uint32_t delta_ticks)
{
    bool pressed = enc_if_btn_raw();
    if (pressed == prev_raw) {
        btn_stable_ticks += delta_ticks;
        if (btn_stable_ticks >= DEBOUNCE_TICKS)
            btn_pressed_stable = pressed;
    } else {
        prev_raw = pressed;
        btn_stable_ticks = 0;
    }
}

void encoder_menu_process(void)
{
    static uint32_t last_tick;
    uint32_t now = enc_if_get_tick_ms();
    uint32_t dt = (now >= last_tick) ? (now - last_tick) : 0;
    last_tick = now;

    /* Завжди забираємо кроки з ISR; ранній return тут виключали — через нього втрачались кроки */
    int step = enc_if_take_encoder_step();
    if (step == 1)
        action = ENCODER_MENU_ACTION_CW;
    else if (step == 2)
        action = ENCODER_MENU_ACTION_CCW;

    enc_if_a_clear_pending();

    poll_button(dt);
    if (btn_pressed_stable && !btn_pressed_prev)
        action = ENCODER_MENU_ACTION_ENTER;
    btn_pressed_prev = btn_pressed_stable;
}

encoder_menu_action_t encoder_menu_get_action(void)
{
    return action;
}

void encoder_menu_clear_action(void)
{
    action = ENCODER_MENU_ACTION_NONE;
}
