#include "encoder_menu.h"
#include "main.h"
#include "enc_if.h"
#include <stdint.h>
#include <stdbool.h>

/* Меню на трьох кнопках (MENU_UP, MENU_DOWN, MENU_ENTER).
 * MENU_UP → вгору (CCW), MENU_DOWN → вниз (CW), MENU_ENTER → Enter.
 * MENU_BTN_ACTIVE_HIGH: 1 = вгору/вниз натиснуто при HIGH. MENU_BTN_ENTER_ACTIVE_HIGH: окремо для PB14 (вибір).
 */
#ifndef MENU_BTN_ACTIVE_HIGH
#define MENU_BTN_ACTIVE_HIGH  0
#endif
#ifndef MENU_BTN_ENTER_ACTIVE_HIGH
#define MENU_BTN_ENTER_ACTIVE_HIGH  1
#endif
#define DEBOUNCE_MS      35u   /* мінімальний час утримання для спрацювання */
#define ACTION_TIMEOUT_MS 1500u /* якщо дію не забрали — скинути, щоб меню не зависало */

static encoder_menu_action_t action;
static uint32_t action_set_tick;

/* Читання сирого рівня: 1 = натиснуто (залежить від MENU_BTN_ACTIVE_HIGH). */
static bool raw_up(void)
{
    return (HAL_GPIO_ReadPin(MENU_UP_GPIO_Port, MENU_UP_Pin) == GPIO_PIN_SET) ? (MENU_BTN_ACTIVE_HIGH != 0) : (MENU_BTN_ACTIVE_HIGH == 0);
}
static bool raw_down(void)
{
    return (HAL_GPIO_ReadPin(MENU_DOWN_GPIO_Port, MENU_DOWN_Pin) == GPIO_PIN_SET) ? (MENU_BTN_ACTIVE_HIGH != 0) : (MENU_BTN_ACTIVE_HIGH == 0);
}
static bool raw_enter(void)
{
    return (HAL_GPIO_ReadPin(MENU_ENTER_GPIO_Port, MENU_ENTER_Pin) == GPIO_PIN_SET) ? (MENU_BTN_ENTER_ACTIVE_HIGH != 0) : (MENU_BTN_ENTER_ACTIVE_HIGH == 0);
}

/* Дебаунс за часом: подія тільки після утримання натиснуто >= DEBOUNCE_MS. primed = спочатку побачили "відпущено". */
typedef struct {
    bool last;
    bool primed;
    uint32_t pressed_since_tick;  /* 0 = не натиснуто */
    bool emitted;
} btn_t;

static btn_t up_btn, down_btn, enter_btn;

static void poll_btn(btn_t *b, bool pressed, encoder_menu_action_t a)
{
    uint32_t now = HAL_GetTick();

    if (pressed) {
        if (!b->last)
            b->pressed_since_tick = now;
        if (b->primed && !b->emitted && action == ENCODER_MENU_ACTION_NONE && b->pressed_since_tick != 0) {
            if ((now - b->pressed_since_tick) >= DEBOUNCE_MS) {
                action = a;
                action_set_tick = now;
                b->emitted = true;
            }
        }
    } else {
        b->pressed_since_tick = 0;
        b->emitted = false;
        b->primed = true;
    }
    b->last = pressed;
}

static void buttons_gpio_init(void)
{
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = MENU_UP_Pin | MENU_DOWN_Pin | MENU_ENTER_Pin;
    HAL_GPIO_Init(MENU_UP_GPIO_Port, &g);
}

void encoder_menu_init(void)
{
    action = ENCODER_MENU_ACTION_NONE;
    action_set_tick = 0;
    up_btn.last = raw_up();
    up_btn.primed = false;
    up_btn.pressed_since_tick = 0;
    up_btn.emitted = false;
    down_btn.last = raw_down();
    down_btn.primed = false;
    down_btn.pressed_since_tick = 0;
    down_btn.emitted = false;
    enter_btn.last = raw_enter();
    enter_btn.primed = false;
    enter_btn.pressed_since_tick = 0;
    enter_btn.emitted = false;
    buttons_gpio_init();
    up_btn.last = raw_up();
    down_btn.last = raw_down();
    enter_btn.last = raw_enter();

    /* Ініціалізація стану енкодера (RE60 на PB12/PB13) для enc_if */
    enc_if_init_encoder_state();
}

void encoder_menu_process(void)
{
    uint32_t now = HAL_GetTick();

    if (action != ENCODER_MENU_ACTION_NONE) {
        if ((now - action_set_tick) >= ACTION_TIMEOUT_MS) {
            action = ENCODER_MENU_ACTION_NONE;
            up_btn.emitted = false;
            down_btn.emitted = false;
            enter_btn.emitted = false;
        } else
            return;
    }

    poll_btn(&up_btn,   raw_up(),   ENCODER_MENU_ACTION_CCW);
    poll_btn(&down_btn,  raw_down(),  ENCODER_MENU_ACTION_CW);
    poll_btn(&enter_btn, raw_enter(), ENCODER_MENU_ACTION_ENTER);
}

encoder_menu_action_t encoder_menu_get_action(void)
{
    return action;
}

void encoder_menu_clear_action(void)
{
    action = ENCODER_MENU_ACTION_NONE;
}
