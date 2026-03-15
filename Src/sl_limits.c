#include "sl_limits.h"
#include "bringup_config.h"
#include "main.h"
#include <float.h>

#ifndef SL_LIMITS_ENABLED
#define SL_LIMITS_ENABLED 1
#endif

#define SL_BTN_ACTIVE_LOW  1  /* натиснуто = LOW */
#define SL_AT_LIMIT_EPS_MM 0.15f
/* Не використовується: усі LED однаково — світяться при HIGH (анод через R на пін, катод на GND). */
#define SL_NEG_LED_ACTIVE_LOW  0

static float s_x_min = 0.0f;
static float s_x_max = 0.0f;
static float s_z_min = 0.0f;
static float s_z_max = 0.0f;
static bool s_x_min_taught = false;
static bool s_x_max_taught = false;
static bool s_z_min_taught = false;
static bool s_z_max_taught = false;
static bool s_test_mode = false;
static bool s_limits_enabled = true;
/* Коротке натискання (< SL_LONG_PRESS_MS) = установка ліміта; довге (≥ SL_LONG_PRESS_MS) = зняття. */
#define SL_LONG_PRESS_MS  600u

void sl_limits_set_test_mode(bool on)
{
    s_test_mode = on;
}

static bool btn_read(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_PinState s = HAL_GPIO_ReadPin(port, pin);
#if SL_BTN_ACTIVE_LOW
    return (s == GPIO_PIN_RESET);
#else
    return (s == GPIO_PIN_SET);
#endif
}

static void led_write(GPIO_TypeDef *port, uint16_t pin, bool on)
{
    HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void sl_limits_init(void)
{
    s_x_min = 0.0f;
    s_x_max = 0.0f;
    s_z_min = 0.0f;
    s_z_max = 0.0f;
    s_x_min_taught = false;
    s_x_max_taught = false;
    s_z_min_taught = false;
    s_z_max_taught = false;
    led_write(SL_X_NEG_LED_GPIO_Port, SL_X_NEG_LED_Pin, false);
    led_write(SL_X_POS_LED_GPIO_Port, SL_X_POS_LED_Pin, false);
    led_write(SL_Z_NEG_LED_GPIO_Port, SL_Z_NEG_LED_Pin, false);
    led_write(SL_Z_POS_LED_GPIO_Port, SL_Z_POS_LED_Pin, false);
}

bool sl_limits_btn_x_neg(void) { return btn_read(SL_X_NEG_BIT_GPIO_Port, SL_X_NEG_BIT_Pin); }
bool sl_limits_btn_x_pos(void) { return btn_read(SL_X_POS_BIT_GPIO_Port, SL_X_POS_BIT_Pin); }
bool sl_limits_btn_z_neg(void) { return btn_read(SL_Z_NEG_BIT_GPIO_Port, SL_Z_NEG_BIT_Pin); }
bool sl_limits_btn_z_pos(void) { return btn_read(SL_Z_POS_BIT_GPIO_Port, SL_Z_POS_BIT_Pin); }

void sl_limits_teach_x_neg(float pos_mm)
{
    s_x_min = pos_mm;
    s_x_min_taught = true;
}
void sl_limits_teach_x_pos(float pos_mm)
{
    s_x_max = pos_mm;
    s_x_max_taught = true;
}
void sl_limits_teach_z_neg(float pos_mm)
{
    s_z_min = pos_mm;
    s_z_min_taught = true;
}
void sl_limits_teach_z_pos(float pos_mm)
{
    s_z_max = pos_mm;
    s_z_max_taught = true;
}

float sl_limits_get_x_min(void) { return s_x_min; }
float sl_limits_get_x_max(void) { return s_x_max; }
float sl_limits_get_z_min(void) { return s_z_min; }
float sl_limits_get_z_max(void) { return s_z_max; }

bool sl_limits_enabled(void) { return s_limits_enabled; }

bool sl_limits_x_taught(void) { return (SL_LIMITS_ENABLED != 0) && s_limits_enabled && s_x_min_taught && s_x_max_taught; }
bool sl_limits_z_taught(void) { return (SL_LIMITS_ENABLED != 0) && s_limits_enabled && s_z_min_taught && s_z_max_taught; }

bool sl_limits_in_range_x(float pos_mm)
{
    if (!s_x_min_taught || !s_x_max_taught) return true;
    float lo = s_x_min < s_x_max ? s_x_min : s_x_max;
    float hi = s_x_min > s_x_max ? s_x_min : s_x_max;
    return pos_mm >= lo - SL_AT_LIMIT_EPS_MM && pos_mm <= hi + SL_AT_LIMIT_EPS_MM;
}

bool sl_limits_in_range_z(float pos_mm)
{
    if (!s_z_min_taught || !s_z_max_taught) return true;
    float lo = s_z_min < s_z_max ? s_z_min : s_z_max;
    float hi = s_z_min > s_z_max ? s_z_min : s_z_max;
    return pos_mm >= lo - SL_AT_LIMIT_EPS_MM && pos_mm <= hi + SL_AT_LIMIT_EPS_MM;
}

void sl_limits_update_leds(float x_mm, float z_mm)
{
    (void)x_mm;
    (void)z_mm;
    if (s_test_mode) {
        /* Режим тесту: натиснуто = LED світить (усі 4 однаково: HIGH = світить) */
        bool xn = sl_limits_btn_x_neg(), xp = sl_limits_btn_x_pos();
        bool zn = sl_limits_btn_z_neg(), zp = sl_limits_btn_z_pos();
        led_write(SL_X_NEG_LED_GPIO_Port, SL_X_NEG_LED_Pin, xn);
        led_write(SL_X_POS_LED_GPIO_Port, SL_X_POS_LED_Pin, xp);
        led_write(SL_Z_NEG_LED_GPIO_Port, SL_Z_NEG_LED_Pin, zn);
        led_write(SL_Z_POS_LED_GPIO_Port, SL_Z_POS_LED_Pin, zp);
        return;
    }
    /* LED світяться при HIGH, коли відповідний ліміт навчено (усі 4 однаково) */
    led_write(SL_X_NEG_LED_GPIO_Port, SL_X_NEG_LED_Pin, s_x_min_taught);
    led_write(SL_X_POS_LED_GPIO_Port, SL_X_POS_LED_Pin, s_x_max_taught);
    led_write(SL_Z_NEG_LED_GPIO_Port, SL_Z_NEG_LED_Pin, s_z_min_taught);
    led_write(SL_Z_POS_LED_GPIO_Port, SL_Z_POS_LED_Pin, s_z_max_taught);
}

void sl_limits_reset(void)
{
    sl_limits_init();
}

/* Коротке натискання = установка ліміта (поточна позиція). Довге натискання (≥ SL_LONG_PRESS_MS) = зняття ліміта. */
void sl_limits_process(float x_mm, float z_mm)
{
    static bool last_xn, last_xp, last_zn, last_zp;
    static uint32_t press_start_xn, press_start_xp, press_start_zn, press_start_zp;  /* 0 = не натиснуто */
    uint32_t now = HAL_GetTick();
    bool xn = sl_limits_btn_x_neg();
    bool xp = sl_limits_btn_x_pos();
    bool zn = sl_limits_btn_z_neg();
    bool zp = sl_limits_btn_z_pos();

    /* X-: при відпусканні — коротке = навчити, довге = зняти */
    if (xn)
        press_start_xn = (press_start_xn != 0u) ? press_start_xn : now;
    else if (last_xn && press_start_xn != 0u) {
        uint32_t dur = (now - press_start_xn);
        if (dur >= SL_LONG_PRESS_MS)
            s_x_min_taught = false;
        else
            sl_limits_teach_x_neg(x_mm);
        press_start_xn = 0u;
    }
    if (!xn) press_start_xn = 0u;

    if (xp)
        press_start_xp = (press_start_xp != 0u) ? press_start_xp : now;
    else if (last_xp && press_start_xp != 0u) {
        uint32_t dur = (now - press_start_xp);
        if (dur >= SL_LONG_PRESS_MS)
            s_x_max_taught = false;
        else
            sl_limits_teach_x_pos(x_mm);
        press_start_xp = 0u;
    }
    if (!xp) press_start_xp = 0u;

    if (zn)
        press_start_zn = (press_start_zn != 0u) ? press_start_zn : now;
    else if (last_zn && press_start_zn != 0u) {
        uint32_t dur = (now - press_start_zn);
        if (dur >= SL_LONG_PRESS_MS)
            s_z_min_taught = false;
        else
            sl_limits_teach_z_neg(z_mm);
        press_start_zn = 0u;
    }
    if (!zn) press_start_zn = 0u;

    if (zp)
        press_start_zp = (press_start_zp != 0u) ? press_start_zp : now;
    else if (last_zp && press_start_zp != 0u) {
        uint32_t dur = (now - press_start_zp);
        if (dur >= SL_LONG_PRESS_MS)
            s_z_max_taught = false;
        else
            sl_limits_teach_z_pos(z_mm);
        press_start_zp = 0u;
    }
    if (!zp) press_start_zp = 0u;

    last_xn = xn;
    last_xp = xp;
    last_zn = zn;
    last_zp = zp;

    sl_limits_update_leds(x_mm, z_mm);
}
