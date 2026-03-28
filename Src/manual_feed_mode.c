#include "manual_feed_mode.h"
#include "board.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

static bool pend_line_low(GPIO_TypeDef *port, uint16_t pin)
{
    return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET;
}

#if MANUAL_FEED_USE_PENDANT_AXIS_SWITCH
/* Усі лінії осей відпущені (підтяжка HIGH) = позиція OFF на пульті = AUTO. */
static bool pendant_axis_is_off(void)
{
    bool x = pend_line_low(ENC_AXIS_S1_GPIO_Port, ENC_AXIS_S1_Pin);
    bool z = pend_line_low(ENC_AXIS_S2_GPIO_Port, ENC_AXIS_S2_Pin);
#if MANUAL_FEED_PENDANT_AXIS_NEED_Y4
    bool y = pend_line_low(PENDAXIS_Y_GPIO_Port, PENDAXIS_Y_Pin);
    bool a4 = pend_line_low(PENDAXIS_4_GPIO_Port, PENDAXIS_4_Pin);
    return !x && !z && !y && !a4;
#else
    return !x && !z;
#endif
}
#endif

void manual_feed_mode_init(void)
{
    /* GPIO для тумблерів уже налаштовані CubeMX у MX_GPIO_Init(). */
}

feed_mode_t manual_feed_get_mode(void)
{
#if MANUAL_FEED_USE_PENDANT_AXIS_SWITCH
    return pendant_axis_is_off() ? FEED_MODE_AUTO : FEED_MODE_MANUAL;
#else
    GPIO_PinState s = HAL_GPIO_ReadPin(FEED_MODE_GPIO_Port, FEED_MODE_Pin);
    /* FEED_MODE (PE2): 0 = AUTO, 1 = MANUAL (інверсія під фактичне підключення тумблера) */
    return (s == GPIO_PIN_SET) ? FEED_MODE_MANUAL : FEED_MODE_AUTO;
#endif
}

encoder_axis_t manual_feed_get_axis(void)
{
#if MANUAL_FEED_USE_PENDANT_AXIS_SWITCH
    if (pendant_axis_is_off())
        return ENCODER_AXIS_NONE;
    bool x = pend_line_low(ENC_AXIS_S1_GPIO_Port, ENC_AXIS_S1_Pin);
    bool z = pend_line_low(ENC_AXIS_S2_GPIO_Port, ENC_AXIS_S2_Pin);
#if MANUAL_FEED_PENDANT_AXIS_NEED_Y4
    bool y = pend_line_low(PENDAXIS_Y_GPIO_Port, PENDAXIS_Y_Pin);
    bool a4 = pend_line_low(PENDAXIS_4_GPIO_Port, PENDAXIS_4_Pin);
#else
    bool y = false;
    bool a4 = false;
#endif
    /* Очікується one-hot: одна лінія до GND через COM перемикача. */
    if (x && !z && !y && !a4)
        return ENCODER_AXIS_X;
    if (!x && z && !y && !a4)
        return ENCODER_AXIS_Z;
    if (y || a4)
        return ENCODER_AXIS_NONE; /* Y / 4 — у jog лише X та Z */
    return ENCODER_AXIS_NONE;
#else
    GPIO_PinState s1 = HAL_GPIO_ReadPin(ENC_AXIS_S1_GPIO_Port, ENC_AXIS_S1_Pin);
    GPIO_PinState s2 = HAL_GPIO_ReadPin(ENC_AXIS_S2_GPIO_Port, ENC_AXIS_S2_Pin);
    bool a = (s1 == GPIO_PIN_RESET);
    bool b = (s2 == GPIO_PIN_RESET);

    /* Приклад: ліво = Z, центр = OFF, право = X */
    if (a && !b) return ENCODER_AXIS_Z;
    if (!a && !b) return ENCODER_AXIS_NONE;
    if (!a && b) return ENCODER_AXIS_X;
    return ENCODER_AXIS_NONE;
#endif
}

step_scale_t manual_feed_get_step_scale(void)
{
#if MANUAL_FEED_USE_PENDANT_STEP_SWITCH
    /* Пульт: окремі дроти X1 / X10 / X100 до GND через COM (одна лінія активна). */
    bool x1 = pend_line_low(PENDSTEP_X1_GPIO_Port, PENDSTEP_X1_Pin);
    bool x10 = pend_line_low(PENDSTEP_X10_GPIO_Port, PENDSTEP_X10_Pin);
    bool x100 = pend_line_low(PENDSTEP_X100_GPIO_Port, PENDSTEP_X100_Pin);
    if (x1)
        return STEP_SCALE_0_001;
    if (x10)
        return STEP_SCALE_0_01;
    if (x100)
        return STEP_SCALE_0_1;
    return STEP_SCALE_0_01;
#else
    GPIO_PinState s1 = HAL_GPIO_ReadPin(ENC_STEP_S1_GPIO_Port, ENC_STEP_S1_Pin);
    GPIO_PinState s2 = HAL_GPIO_ReadPin(ENC_STEP_S2_GPIO_Port, ENC_STEP_S2_Pin);
    bool a = (s1 == GPIO_PIN_RESET);
    bool b = (s2 == GPIO_PIN_RESET);

    /* Ліво = 0.001, центр = 0.01, право = 0.1 мм */
    if (a && !b) return STEP_SCALE_0_001;
    if (!a && !b) return STEP_SCALE_0_01;
    if (!a && b) return STEP_SCALE_0_1;
    return STEP_SCALE_0_01;
#endif
}

