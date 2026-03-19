#include "linenc_zero_buttons.h"
#include "axis_feedback.h"
#include "bringup_config.h"
#include "main.h"

#if BRINGUP_MODE

/* Debounce and edge-detect for separate ZERO buttons.
 * We keep this completely separate from menu buttons.
 */

#ifndef LIN_ZERO_DEBOUNCE_MS
#define LIN_ZERO_DEBOUNCE_MS 35u
#endif

static uint32_t s_x_last_change;
static uint32_t s_z_last_change;
static uint8_t s_x_last_raw;
static uint8_t s_z_last_raw;
static uint8_t s_x_stable;
static uint8_t s_z_stable;

static uint8_t read_pressed_x(void)
{
#ifdef LIN_ZERO_X_Pin
    return (HAL_GPIO_ReadPin(LIN_ZERO_X_GPIO_Port, LIN_ZERO_X_Pin) == GPIO_PIN_RESET) ? 1u : 0u;
#else
    return 0u;
#endif
}

static uint8_t read_pressed_z(void)
{
#ifdef LIN_ZERO_Z_Pin
    return (HAL_GPIO_ReadPin(LIN_ZERO_Z_GPIO_Port, LIN_ZERO_Z_Pin) == GPIO_PIN_RESET) ? 1u : 0u;
#else
    return 0u;
#endif
}

void linenc_zero_buttons_init(void)
{
    s_x_last_change = 0u;
    s_z_last_change = 0u;
    s_x_last_raw = read_pressed_x();
    s_z_last_raw = read_pressed_z();
    s_x_stable = s_x_last_raw;
    s_z_stable = s_z_last_raw;
}

void linenc_zero_buttons_process(void)
{
    uint32_t now = HAL_GetTick();

    /* X */
    {
        uint8_t raw = read_pressed_x();
        if (raw != s_x_last_raw) {
            s_x_last_raw = raw;
            s_x_last_change = now;
        }
        if ((now - s_x_last_change) >= LIN_ZERO_DEBOUNCE_MS) {
            if (raw != s_x_stable) {
                /* rising edge of "pressed" -> action */
                if (raw == 1u) axis_feedback_zero(AXIS_X);
                s_x_stable = raw;
            }
        }
    }

    /* Z */
    {
        uint8_t raw = read_pressed_z();
        if (raw != s_z_last_raw) {
            s_z_last_raw = raw;
            s_z_last_change = now;
        }
        if ((now - s_z_last_change) >= LIN_ZERO_DEBOUNCE_MS) {
            if (raw != s_z_stable) {
                if (raw == 1u) axis_feedback_zero(AXIS_Z);
                s_z_stable = raw;
            }
        }
    }
}

#else
void linenc_zero_buttons_init(void) {}
void linenc_zero_buttons_process(void) {}
#endif

