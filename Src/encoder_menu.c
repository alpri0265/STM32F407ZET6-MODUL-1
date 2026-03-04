#include "encoder_menu.h"
#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* Проста реалізація меню на трьох кнопках замість енкодера.
 *
 * PB12 (MPG_A_Pin)  → MENU_UP    → ENCODER_MENU_ACTION_CCW  (рух курсора вгору)
 * PB13 (MPG_B_Pin)  → MENU_DOWN  → ENCODER_MENU_ACTION_CW   (рух курсора вниз)
 * PB14 (MPG_BTN_Pin)→ MENU_ENTER → ENCODER_MENU_ACTION_ENTER
 *
 * Кнопки підключені до землі (натиснуто = LOW), всередині ввімкнена підтяжка вгору.
 */

#define DEBOUNCE_MS    30u

static encoder_menu_action_t action;

typedef struct {
    bool prev_raw;
    bool stable;
    bool prev_stable;
    uint32_t stable_ticks;
} btn_state_t;

static btn_state_t btn_up;
static btn_state_t btn_down;
static btn_state_t btn_enter;

static bool read_btn_up(void)
{
    return HAL_GPIO_ReadPin(MPG_A_GPIO_Port, MPG_A_Pin) == GPIO_PIN_RESET;
}

static bool read_btn_down(void)
{
    return HAL_GPIO_ReadPin(MPG_B_GPIO_Port, MPG_B_Pin) == GPIO_PIN_RESET;
}

static bool read_btn_enter(void)
{
    return HAL_GPIO_ReadPin(MPG_BTN_GPIO_Port, MPG_BTN_Pin) == GPIO_PIN_RESET;
}

static void btn_poll(btn_state_t *s, bool raw, uint32_t dt_ms)
{
    if (raw == s->prev_raw) {
        if (s->stable_ticks < DEBOUNCE_MS)
            s->stable_ticks += dt_ms;
        if (s->stable_ticks >= DEBOUNCE_MS)
            s->stable = raw;
    } else {
        s->prev_raw = raw;
        s->stable_ticks = 0;
    }
}

void encoder_menu_init(void)
{
    action = ENCODER_MENU_ACTION_NONE;

    btn_up.prev_raw = read_btn_up();
    btn_up.stable = false;
    btn_up.prev_stable = false;
    btn_up.stable_ticks = 0;

    btn_down.prev_raw = read_btn_down();
    btn_down.stable = false;
    btn_down.prev_stable = false;
    btn_down.stable_ticks = 0;

    btn_enter.prev_raw = read_btn_enter();
    btn_enter.stable = false;
    btn_enter.prev_stable = false;
    btn_enter.stable_ticks = 0;
}

void encoder_menu_process(void)
{
    static uint32_t last_tick;
    uint32_t now = HAL_GetTick();
    uint32_t dt = (now >= last_tick) ? (now - last_tick) : 0;
    if (dt > 100u) dt = 100u; /* обмеження, щоб не переповнювати лічильники при паузах */
    last_tick = now;

    /* Якщо попередня дія ще не оброблена екранами — не приймаємо нові події */
    if (action != ENCODER_MENU_ACTION_NONE)
        return;

    btn_poll(&btn_up,    read_btn_up(),    dt);
    btn_poll(&btn_down,  read_btn_down(),  dt);
    btn_poll(&btn_enter, read_btn_enter(), dt);

    /* Фронти натискання (stable: 0→1) */
    if (btn_up.stable && !btn_up.prev_stable) {
        action = ENCODER_MENU_ACTION_CCW;   /* курсор вгору */
    } else if (btn_down.stable && !btn_down.prev_stable) {
        action = ENCODER_MENU_ACTION_CW;    /* курсор вниз */
    } else if (btn_enter.stable && !btn_enter.prev_stable) {
        action = ENCODER_MENU_ACTION_ENTER;
    }

    btn_up.prev_stable    = btn_up.stable;
    btn_down.prev_stable  = btn_down.stable;
    btn_enter.prev_stable = btn_enter.stable;
}

encoder_menu_action_t encoder_menu_get_action(void)
{
    return action;
}

void encoder_menu_clear_action(void)
{
    action = ENCODER_MENU_ACTION_NONE;
}
