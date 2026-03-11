#include "jog.h"
#include "main.h"
#include "bringup_config.h"
#include "menu.h"
#include <stdint.h>
#include <stdbool.h>

#if BRINGUP_MODE

/* Підключення до драйвера (DM556): PUL = STEP, DIR = напрямок, ENA = дозвіл.
   В main.h: X — PA8=PUL, PA9=DIR, PA10=ENA; Z — PB6=PUL, PB7=DIR, PB8=ENA. */
#define JOG_STEP_PERIOD_MS  2u     /* мс між кроками (~500 кроків/с) */
#define JOG_PULSE_CYCLES    8000u  /* тривалість імпульсу ~48 µs при 168 MHz (деякі драйвери потребують 10–50 µs) */
#define JOG_DIR_SETTLE      400u   /* циклів після встановлення DIR перед STEP (~2.4 µs) */

/* 1 = імпульс кроку активний по LOW (idle HIGH, pulse LOW); 0 = активний по HIGH */
#define STEP_PULSE_ACTIVE_LOW  1
/* 1 = ENA по HIGH увімкнено; 0 = ENA по LOW увімкнено (типово для DM556) */
#define ENA_ACTIVE_HIGH        0
/* 1 = на екрані Jog постійно кроки X/Z без джойстика (тест проводки/драйвера). Потім поставити 0. */
#define JOG_ALWAYS_RUN_TEST    1

static uint32_t s_last_step_tick;
static uint32_t s_step_count_x;
static uint32_t s_step_count_z;

static void step_pulse_delay(void)
{
    for (volatile uint32_t i = 0u; i < JOG_PULSE_CYCLES; i++) (void)0;
}

static void dir_settle_delay(void)
{
    for (volatile uint32_t i = 0u; i < JOG_DIR_SETTLE; i++) (void)0;
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
#endif

void jog_process(void)
{
    if (menu_current_screen() != SCREEN_JOG)
        return;
    uint32_t now = HAL_GetTick();
    if ((now - s_last_step_tick) < JOG_STEP_PERIOD_MS)
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
            pulse_x_step();
        } else if (down) {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
            pulse_x_step();
        }
    } else {
        if (up) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
            pulse_z_step();
        } else if (down) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
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
#endif
