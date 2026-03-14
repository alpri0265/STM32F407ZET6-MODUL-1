#include "jog.h"
#include "main.h"
#include "board.h"
#include "adc_if.h"
#include "bringup_config.h"
#include "menu.h"
#include <stdint.h>
#include <stdbool.h>

#if BRINGUP_MODE

/* Підключення до драйвера (DM556): PUL = STEP, DIR = напрямок, ENA = дозвіл.
   В main.h: X — PA8=PUL, PA9=DIR, PA10=ENA; Z — PB6=PUL, PB7=DIR, PB8=ENA. */
#define JOG_STEP_PERIOD_MS       4u  /* мс між кроками (база) — більший діапазон для rapid */
#define JOG_STEP_PERIOD_RAPID_MS 1u  /* мс при rapid — завжди 2–4× швидше */
/* Feed override: ADC 0..4095 → scale 50..150%. period = base*100/scale, min 1 ms */
#define JOG_PULSE_CYCLES    8000u  /* тривалість імпульсу ~48 µs при 168 MHz */
#define JOG_DIR_SETTLE      400u   /* циклів після DIR перед STEP (~2.4 µs) */

/* 1 = імпульс кроку активний по LOW (idle HIGH, pulse LOW); 0 = активний по HIGH */
#define STEP_PULSE_ACTIVE_LOW  0
/* 1 = ENA по HIGH увімкнено; 0 = ENA по LOW увімкнено */
#define ENA_ACTIVE_HIGH        1
/* 1 = постійні кроки X/Z без джойстика (тест); 0 = рух тільки від джойстика */
#define JOG_ALWAYS_RUN_TEST    0
/* Кнопка пришвидшеної подачі: використовуємо SCALE_0 (PC0). Натиснуто = LOW (на землю). */
#define RAPID_BTN_GPIO_Port    SCALE_0_GPIO_Port
#define RAPID_BTN_Pin          SCALE_0_Pin
#define RAPID_BTN_ACTIVE_LOW   1

static uint32_t s_last_step_tick;
static uint32_t s_step_count_x;
static uint32_t s_step_count_z;
static uint16_t s_feed_override_cached = 2048u;  /* 50% по замовчуванню, оновлюється з main loop */

static void step_pulse_delay(void)
{
    for (volatile uint32_t i = 0u; i < JOG_PULSE_CYCLES; i++) (void)0;
}

static void dir_settle_delay(void)
{
    for (volatile uint32_t i = 0u; i < JOG_DIR_SETTLE; i++) (void)0;
}

static bool rapid_pressed(void)
{
    GPIO_PinState s = HAL_GPIO_ReadPin(RAPID_BTN_GPIO_Port, RAPID_BTN_Pin);
    if (RAPID_BTN_ACTIVE_LOW)
        return (s == GPIO_PIN_RESET);
    else
        return (s == GPIO_PIN_SET);
}

static void pulse_x_step(void)
{
#if STEP_PULSE_ACTIVE_LOW
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_RESET);
    step_pulse_delay();
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_SET);
    step_pulse_delay();
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_RESET);
#endif
    s_step_count_x++;
}

static void pulse_z_step(void)
{
#if STEP_PULSE_ACTIVE_LOW
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_RESET);
    step_pulse_delay();
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_SET);
    step_pulse_delay();
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_RESET);
#endif
    s_step_count_z++;
}

void jog_init(void)
{
    s_last_step_tick = 0u;
    /* STEP (PUL) — переводимо з AF TIM1/TIM4 у звичайний вихід GPIO */
    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Pin = X_STEP_Pin;
    HAL_GPIO_Init(X_STEP_GPIO_Port, &g);
    g.Pin = z_STEP_Pin;
    HAL_GPIO_Init(z_STEP_GPIO_Port, &g);
#if STEP_PULSE_ACTIVE_LOW
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(X_STEP_GPIO_Port, X_STEP_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(z_STEP_GPIO_Port, z_STEP_Pin, GPIO_PIN_RESET);
#endif
#if ENA_ACTIVE_HIGH
    HAL_GPIO_WritePin(X_EN_GPIO_Port, X_EN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(Z_EN_GPIO_Port, Z_EN_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(X_EN_GPIO_Port, X_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(Z_EN_GPIO_Port, Z_EN_Pin, GPIO_PIN_RESET);
#endif

    /* Кнопка Rapid (SCALE_0 / PC0): вхід з підтяжкою вгору, натиснуто = до GND. */
    GPIO_InitTypeDef b = {0};
    b.Pin = RAPID_BTN_Pin;
    b.Mode = GPIO_MODE_INPUT;
    b.Pull = GPIO_PULLUP;
    b.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RAPID_BTN_GPIO_Port, &b);
}

/* 1 = рух від джойстика (JOY_UP/DOWN/LEFT/RIGHT), 0 = рух від кнопок меню */
#define JOG_USE_JOYSTICK 1

#if JOG_USE_JOYSTICK
/* 1 = натиснуто коли пін LOW, 0 = натиснуто коли пін HIGH. За показами U/D/L/R на екрані Jog */
#define JOY_UP_ACTIVE_LOW    1
#define JOY_DOWN_ACTIVE_LOW  1
#define JOY_LEFT_ACTIVE_LOW  1
#define JOY_RIGHT_ACTIVE_LOW 1
static bool joy_up(void)    { return (HAL_GPIO_ReadPin(JOY_UP_GPIO_Port, JOY_UP_Pin) == GPIO_PIN_RESET) == (JOY_UP_ACTIVE_LOW != 0); }
static bool joy_down(void)  { return (HAL_GPIO_ReadPin(JOY_DOWN_GPIO_Port, JOY_DOWN_Pin) == GPIO_PIN_RESET) == (JOY_DOWN_ACTIVE_LOW != 0); }
static bool joy_left(void)  { return (HAL_GPIO_ReadPin(JOY_LEFT_GPIO_Port, JOY_LEFT_Pin) == GPIO_PIN_RESET) == (JOY_LEFT_ACTIVE_LOW != 0); }
static bool joy_right(void) { return (HAL_GPIO_ReadPin(JOY_RIGHT_GPIO_Port, JOY_RIGHT_Pin) == GPIO_PIN_RESET) == (JOY_RIGHT_ACTIVE_LOW != 0); }

void jog_get_joy_state(unsigned int *up, unsigned int *down, unsigned int *left, unsigned int *right)
{
    if (up)   *up   = joy_up()   ? 1u : 0u;
    if (down) *down = joy_down() ? 1u : 0u;
    if (left) *left = joy_left() ? 1u : 0u;
    if (right) *right = joy_right() ? 1u : 0u;
}

void jog_get_step_counts(uint32_t *x, uint32_t *z)
{
    if (x) *x = s_step_count_x;
    if (z) *z = s_step_count_z;
}

void jog_get_rapid_state(unsigned int *rapid)
{
    if (rapid) *rapid = rapid_pressed() ? 1u : 0u;
}

void jog_set_feed_override(uint16_t raw)
{
    s_feed_override_cached = raw;
}
#else
void jog_get_joy_state(unsigned int *up, unsigned int *down, unsigned int *left, unsigned int *right)
{
    if (up)   *up   = 0u;
    if (down) *down = 0u;
    if (left) *left = 0u;
    if (right) *right = 0u;
}
void jog_get_step_counts(uint32_t *x, uint32_t *z)
{
    if (x) *x = s_step_count_x;
    if (z) *z = s_step_count_z;
}
void jog_get_rapid_state(unsigned int *rapid)
{
    if (rapid) *rapid = 0u;
}
void jog_set_feed_override(uint16_t raw)
{
    s_feed_override_cached = raw;
}
#endif

void jog_process(void)
{
    if (menu_current_screen() != SCREEN_JOG)
        return;
    uint32_t now = HAL_GetTick();
    uint32_t base_ms = rapid_pressed() ? JOG_STEP_PERIOD_RAPID_MS : JOG_STEP_PERIOD_MS;
    uint16_t feed_raw = s_feed_override_cached;  /* оновлюється з main loop, не з ISR */
    /* Feed 0..4095 → scale 30..150%, period = base*100/scale, min 1 */
    uint32_t scale = 30u + ((uint32_t)feed_raw * 120u) / 4095u;
    if (scale < 10u) scale = 10u;
    uint32_t period_ms = (base_ms * 100u) / scale;
    if (period_ms < 1u) period_ms = 1u;

    if ((now - s_last_step_tick) < period_ms)
        return;

    s_last_step_tick = now;

#if JOG_ALWAYS_RUN_TEST
    /* Тест без джойстика: постійно кроки X та Z по черзі. Якщо двигуни рухаються — проводка/драйвер ОК, постав JOG_ALWAYS_RUN_TEST 0. */
    {
        static int alt;
        if (alt) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_z_step();
        } else {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_x_step();
        }
        alt = 1 - alt;
        return;
    }
#endif

#if JOG_USE_JOYSTICK
    /* Пріоритет U, D, L, R — при замиканні двох контактів (напр. U+R) рух по головному напрямку */
    if (joy_up()) {
        HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
        dir_settle_delay();
        pulse_z_step();
        return;
    }
    if (joy_down()) {
        HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
        dir_settle_delay();
        pulse_z_step();
        return;
    }
    if (joy_left()) {
        HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
        dir_settle_delay();
        pulse_x_step();
        return;
    }
    if (joy_right()) {
        HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
        dir_settle_delay();
        pulse_x_step();
        return;
    }
#else
    /* Режим кнопок меню: Enter перемикає вісь (X/Z), Up = +, Down = −. PB12=Up, PB13=Down, PB14=Enter */
    static int axis_mode; /* 0=X, 1=Z */
    static bool last_ent;
    bool up   = (HAL_GPIO_ReadPin(MPG_A_GPIO_Port, MPG_A_Pin) == GPIO_PIN_RESET);
    bool down = (HAL_GPIO_ReadPin(MPG_B_GPIO_Port, MPG_B_Pin) == GPIO_PIN_RESET);
    bool ent  = (HAL_GPIO_ReadPin(MPG_BTN_GPIO_Port, MPG_BTN_Pin) == GPIO_PIN_SET);

    if (ent && !last_ent) {
        axis_mode = 1 - axis_mode;
        s_last_step_tick = now;
    }
    last_ent = ent;
    if (ent) return;
    if (axis_mode == 0) {
        if (up) {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_x_step();
        } else if (down) {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_x_step();
        }
    } else {
        if (up) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_z_step();
        } else if (down) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_z_step();
        }
    }
#endif
}

#else
void jog_init(void) {}
void jog_process(void) {}
void jog_get_joy_state(unsigned int *up, unsigned int *down, unsigned int *left, unsigned int *right)
{
    if (up)   *up   = 0u;
    if (down) *down = 0u;
    if (left) *left = 0u;
    if (right) *right = 0u;
}
void jog_get_step_counts(uint32_t *x, uint32_t *z)
{
    if (x) *x = 0u;
    if (z) *z = 0u;
}
void jog_get_rapid_state(unsigned int *rapid)
{
    if (rapid) *rapid = 0u;
}
void jog_set_feed_override(uint16_t raw) { (void)raw; }
#endif
