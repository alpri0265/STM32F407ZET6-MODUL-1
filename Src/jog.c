#include "jog.h"
#include "main.h"
#include "board.h"
#include "adc_if.h"
#include "bringup_config.h"
#include "menu.h"
#include "system_config.h"
#include "sl_limits.h"
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
#define SL_OSC_HYST_MM      1.0f   /* гістерезис при розвороті (мм) — уникнути вібрації */
#define SL_OSC_MIN_RANGE_MM 2.0f   /* мін. відстань між лімітами для авто-коливань */

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
/* Підписані позиції в кроках для програмних лімітів */
static int32_t s_pos_x_steps;
static int32_t s_pos_z_steps;
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

static void pulse_x_step(int dir)
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
    s_pos_x_steps += dir;
}

static void pulse_z_step(int dir)
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
    s_pos_z_steps += dir;
}

void jog_init(void)
{
    s_last_step_tick = 0u;
    s_pos_x_steps = 0;
    s_pos_z_steps = 0;
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

void jog_get_pos_mm(float *x_mm, float *z_mm)
{
    const axis_cfg_t *cx = system_axis_cfg(AXIS_X);
    const axis_cfg_t *cz = system_axis_cfg(AXIS_Z);
    if (x_mm) *x_mm = (float)s_pos_x_steps / (cx->steps_per_mm > 0.0f ? cx->steps_per_mm : 1.0f);
    if (z_mm) *z_mm = (float)s_pos_z_steps / (cz->steps_per_mm > 0.0f ? cz->steps_per_mm : 1.0f);
}

void jog_get_rapid_state(unsigned int *rapid)
{
    if (rapid) *rapid = rapid_pressed() ? 1u : 0u;
}

void jog_set_feed_override(uint16_t raw)
{
    s_feed_override_cached = raw;
}

/* Мінімальний крок від джойстика — БЕЗ меню, без sl_limits. Викликати з TIM6. */
void jog_tick_from_isr(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t base_ms = rapid_pressed() ? JOG_STEP_PERIOD_RAPID_MS : JOG_STEP_PERIOD_MS;
    uint16_t feed_raw = s_feed_override_cached;
    uint32_t scale = 30u + ((uint32_t)feed_raw * 120u) / 4095u;
    if (scale < 10u) scale = 10u;
    uint32_t period_ms = (base_ms * 100u) / scale;
    if (period_ms < 1u) period_ms = 1u;
    if ((now - s_last_step_tick) < period_ms)
        return;
    s_last_step_tick = now;

    if (joy_up()) {
        HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
        dir_settle_delay();
        pulse_z_step(1);
        return;
    }
    if (joy_down()) {
        HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
        dir_settle_delay();
        pulse_z_step(-1);
        return;
    }
    if (joy_left()) {
        HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
        dir_settle_delay();
        pulse_x_step(-1);
        return;
    }
    if (joy_right()) {
        HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
        dir_settle_delay();
        pulse_x_step(1);
    }
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
void jog_get_pos_mm(float *x_mm, float *z_mm)
{
    const axis_cfg_t *cx = system_axis_cfg(AXIS_X);
    const axis_cfg_t *cz = system_axis_cfg(AXIS_Z);
    if (x_mm) *x_mm = (float)s_pos_x_steps / (cx->steps_per_mm > 0.0f ? cx->steps_per_mm : 1.0f);
    if (z_mm) *z_mm = (float)s_pos_z_steps / (cz->steps_per_mm > 0.0f ? cz->steps_per_mm : 1.0f);
}
void jog_get_rapid_state(unsigned int *rapid)
{
    if (rapid) *rapid = 0u;
}
void jog_set_feed_override(uint16_t raw)
{
    s_feed_override_cached = raw;
}
void jog_tick_from_isr(void) { (void)0; }  /* заглушка: джойстик не використовується */
#endif

void jog_process(void)
{
    /* Джойстик працює завжди; scr лише для Feed Auto vs ручний режим. */
    menu_screen_id_t scr = menu_current_screen();
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
            pulse_z_step(1);
        } else {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_x_step(1);
        }
        alt = 1 - alt;
        return;
    }
#endif

#if JOG_USE_JOYSTICK
    if (scr == SCREEN_FEED_AUTO) {
        /* Feed Auto: нейтраль = стіп. Джойстик натиснуто + ліміти навчені = авто-коливання.
           Джойстик натиснуто + ліміти НЕ навчені = ручний рух (навчання). */
        float xm, zm;
        jog_get_pos_mm(&xm, &zm);
        static int x_dir = 1, z_dir = 1;

        if (!joy_up() && !joy_down() && !joy_left() && !joy_right()) {
            return;  /* Нейтраль — зупинка */
        }

        /* Джойстик натиснуто */
        if (joy_up()) {
            if (sl_limits_z_taught()) {
                float lo = sl_limits_get_z_min();
                float hi = sl_limits_get_z_max();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if ((hi - lo) < SL_OSC_MIN_RANGE_MM) return; /* занадто близькі ліміти — не коливати */
                if (zm >= hi - SL_OSC_HYST_MM) z_dir = -1;
                else if (zm <= lo + SL_OSC_HYST_MM) z_dir = 1;
                HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, z_dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
                dir_settle_delay();
                pulse_z_step(z_dir);
            } else {
                HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
                dir_settle_delay();
                pulse_z_step(1);
            }
            return;
        }
        if (joy_down()) {
            if (sl_limits_z_taught()) {
                float lo = sl_limits_get_z_min();
                float hi = sl_limits_get_z_max();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if ((hi - lo) < SL_OSC_MIN_RANGE_MM) return;
                if (zm >= hi - SL_OSC_HYST_MM) z_dir = -1;
                else if (zm <= lo + SL_OSC_HYST_MM) z_dir = 1;
                HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, z_dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
                dir_settle_delay();
                pulse_z_step(z_dir);
            } else {
                HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
                dir_settle_delay();
                pulse_z_step(-1);
            }
            return;
        }
        if (joy_left()) {
            if (sl_limits_x_taught()) {
                float lo = sl_limits_get_x_min();
                float hi = sl_limits_get_x_max();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if ((hi - lo) < SL_OSC_MIN_RANGE_MM) return;
                if (xm >= hi - SL_OSC_HYST_MM) x_dir = -1;
                else if (xm <= lo + SL_OSC_HYST_MM) x_dir = 1;
                HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, x_dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
                dir_settle_delay();
                pulse_x_step(x_dir);
            } else {
                HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
                dir_settle_delay();
                pulse_x_step(-1);
            }
            return;
        }
        if (joy_right()) {
            if (sl_limits_x_taught()) {
                float lo = sl_limits_get_x_min();
                float hi = sl_limits_get_x_max();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if ((hi - lo) < SL_OSC_MIN_RANGE_MM) return;
                if (xm >= hi - SL_OSC_HYST_MM) x_dir = -1;
                else if (xm <= lo + SL_OSC_HYST_MM) x_dir = 1;
                HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, x_dir > 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
                dir_settle_delay();
                pulse_x_step(x_dir);
            } else {
                HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
                dir_settle_delay();
                pulse_x_step(1);
            }
            return;
        }
    }

    /* Jog: ручний рух з блокуванням за межами лімітів */
    {
        float xm, zm;
        jog_get_pos_mm(&xm, &zm);
        if (joy_up()) {
            if (sl_limits_z_taught()) {
                float hi = sl_limits_get_z_max();
                float lo = sl_limits_get_z_min();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if (zm >= hi - 0.2f) return;
            }
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_z_step(1);
            return;
        }
        if (joy_down()) {
            if (sl_limits_z_taught()) {
                float hi = sl_limits_get_z_max();
                float lo = sl_limits_get_z_min();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if (zm <= lo + 0.2f) return;
            }
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_z_step(-1);
            return;
        }
        if (joy_left()) {
            if (sl_limits_x_taught()) {
                float hi = sl_limits_get_x_max();
                float lo = sl_limits_get_x_min();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if (xm <= lo + 0.2f) return;
            }
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_x_step(-1);
            return;
        }
        if (joy_right()) {
            if (sl_limits_x_taught()) {
                float hi = sl_limits_get_x_max();
                float lo = sl_limits_get_x_min();
                if (lo > hi) { float t = lo; lo = hi; hi = t; }
                if (xm >= hi - 0.2f) return;
            }
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_x_step(1);
            return;
        }
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
            pulse_x_step(1);
        } else if (down) {
            HAL_GPIO_WritePin(X_DIR_GPIO_Port, X_DIR_Pin, GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_x_step(-1);
        }
    } else {
        if (up) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_SET);
            dir_settle_delay();
            pulse_z_step(1);
        } else if (down) {
            HAL_GPIO_WritePin(Z_DIR_GPIO_Port, Z_DIR_Pin, GPIO_PIN_RESET);
            dir_settle_delay();
            pulse_z_step(-1);
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
void jog_get_pos_mm(float *x_mm, float *z_mm)
{
    if (x_mm) *x_mm = 0.0f;
    if (z_mm) *z_mm = 0.0f;
}
void jog_get_rapid_state(unsigned int *rapid)
{
    if (rapid) *rapid = 0u;
}
void jog_set_feed_override(uint16_t raw) { (void)raw; }
#endif
