#include "sl_limits.h"
#include "main.h"
#include <float.h>

#define SL_BTN_ACTIVE_LOW  1  /* натиснуто = LOW */
#define SL_AT_LIMIT_EPS_MM 0.15f

static float s_x_min = 0.0f;
static float s_x_max = 0.0f;
static float s_z_min = 0.0f;
static float s_z_max = 0.0f;
static bool s_x_min_taught = false;
static bool s_x_max_taught = false;
static bool s_z_min_taught = false;
static bool s_z_max_taught = false;

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

bool sl_limits_x_taught(void) { return s_x_min_taught && s_x_max_taught; }
bool sl_limits_z_taught(void) { return s_z_min_taught && s_z_max_taught; }

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
    /* LED світяться, коли відповідний ліміт навчено встановлений */
    led_write(SL_X_NEG_LED_GPIO_Port, SL_X_NEG_LED_Pin, s_x_min_taught);
    led_write(SL_X_POS_LED_GPIO_Port, SL_X_POS_LED_Pin, s_x_max_taught);
    led_write(SL_Z_NEG_LED_GPIO_Port, SL_Z_NEG_LED_Pin, s_z_min_taught);
    led_write(SL_Z_POS_LED_GPIO_Port, SL_Z_POS_LED_Pin, s_z_max_taught);
}

void sl_limits_reset(void)
{
    sl_limits_init();
}

/* Обробка в режимі Feed Auto: натискання кнопок = навчання, оновлення LED.
   Викликати з app з поточними x_mm, z_mm. */
void sl_limits_process(float x_mm, float z_mm)
{
    static bool last_xn, last_xp, last_zn, last_zp;
    bool xn = sl_limits_btn_x_neg();
    bool xp = sl_limits_btn_x_pos();
    bool zn = sl_limits_btn_z_neg();
    bool zp = sl_limits_btn_z_pos();

    if (xn && !last_xn) sl_limits_teach_x_neg(x_mm);
    if (xp && !last_xp) sl_limits_teach_x_pos(x_mm);
    if (zn && !last_zn) sl_limits_teach_z_neg(z_mm);
    if (zp && !last_zp) sl_limits_teach_z_pos(z_mm);

    last_xn = xn;
    last_xp = xp;
    last_zn = zn;
    last_zp = zp;

    sl_limits_update_leds(x_mm, z_mm);
}
