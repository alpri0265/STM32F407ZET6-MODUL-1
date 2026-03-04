#include "encoder_menu.h"
#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* Меню на трьох кнопках (PB12, PB13, PB14).
 * PB12 → вгору (CCW), PB13 → вниз (CW), PB14 → Enter.
 * MENU_BTN_ACTIVE_HIGH: 1 = вгору/вниз натиснуто при HIGH. MENU_BTN_ENTER_ACTIVE_HIGH: окремо для PB14 (вибір).
 */
#ifndef MENU_BTN_ACTIVE_HIGH
#define MENU_BTN_ACTIVE_HIGH  0
#endif
#ifndef MENU_BTN_ENTER_ACTIVE_HIGH
#define MENU_BTN_ENTER_ACTIVE_HIGH  1
#endif

static encoder_menu_action_t action;

/* Читання сирого рівня: 1 = натиснуто (залежить від MENU_BTN_ACTIVE_HIGH). */
static bool raw_up(void)
{
    return (HAL_GPIO_ReadPin(MPG_A_GPIO_Port, MPG_A_Pin) == GPIO_PIN_SET) ? (MENU_BTN_ACTIVE_HIGH != 0) : (MENU_BTN_ACTIVE_HIGH == 0);
}
static bool raw_down(void)
{
    return (HAL_GPIO_ReadPin(MPG_B_GPIO_Port, MPG_B_Pin) == GPIO_PIN_SET) ? (MENU_BTN_ACTIVE_HIGH != 0) : (MENU_BTN_ACTIVE_HIGH == 0);
}
static bool raw_enter(void)
{
    return (HAL_GPIO_ReadPin(MPG_BTN_GPIO_Port, MPG_BTN_Pin) == GPIO_PIN_SET) ? (MENU_BTN_ENTER_ACTIVE_HIGH != 0) : (MENU_BTN_ENTER_ACTIVE_HIGH == 0);
}

/* Подія на перехід "відпущено" -> "натиснуто". primed = побачили "відпущено" хоча б раз (щоб не спрацювало при утриманні під час включення). */
typedef struct {
    bool last;
    bool primed;
} btn_t;

static btn_t up_btn, down_btn, enter_btn;

static void poll_btn(btn_t *b, bool pressed, encoder_menu_action_t a)
{
    if (pressed && !b->last && b->primed && action == ENCODER_MENU_ACTION_NONE)
        action = a;
    b->last = pressed;
    if (!pressed)
        b->primed = true;
}

static void buttons_gpio_init(void)
{
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin = MPG_A_Pin | MPG_B_Pin | MPG_BTN_Pin;
    HAL_GPIO_Init(MPG_A_GPIO_Port, &g);
}

void encoder_menu_init(void)
{
    action = ENCODER_MENU_ACTION_NONE;
    up_btn.last = raw_up();
    up_btn.primed = false;
    down_btn.last = raw_down();
    down_btn.primed = false;
    enter_btn.last = raw_enter();
    enter_btn.primed = false;
    buttons_gpio_init();
    up_btn.last = raw_up();
    down_btn.last = raw_down();
    enter_btn.last = raw_enter();
}

void encoder_menu_process(void)
{
    if (action != ENCODER_MENU_ACTION_NONE)
        return;

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
