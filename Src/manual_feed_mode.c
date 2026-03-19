#include "manual_feed_mode.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

void manual_feed_mode_init(void)
{
    /* GPIO для тумблерів уже налаштовані CubeMX у MX_GPIO_Init(). */
}

feed_mode_t manual_feed_get_mode(void)
{
    GPIO_PinState s = HAL_GPIO_ReadPin(FEED_MODE_GPIO_Port, FEED_MODE_Pin);
    /* FEED_MODE (PE2): 0 = AUTO, 1 = MANUAL (інверсія під фактичне підключення тумблера) */
    return (s == GPIO_PIN_SET) ? FEED_MODE_MANUAL : FEED_MODE_AUTO;
}

encoder_axis_t manual_feed_get_axis(void)
{
    GPIO_PinState s1 = HAL_GPIO_ReadPin(ENC_AXIS_S1_GPIO_Port, ENC_AXIS_S1_Pin);
    GPIO_PinState s2 = HAL_GPIO_ReadPin(ENC_AXIS_S2_GPIO_Port, ENC_AXIS_S2_Pin);
    bool a = (s1 == GPIO_PIN_RESET);
    bool b = (s2 == GPIO_PIN_RESET);

    /* Приклад: ліво = Z, центр = OFF, право = X */
    if (a && !b) return ENCODER_AXIS_Z;
    if (!a && !b) return ENCODER_AXIS_NONE;
    if (!a && b) return ENCODER_AXIS_X;
    return ENCODER_AXIS_NONE;
}

step_scale_t manual_feed_get_step_scale(void)
{
    GPIO_PinState s1 = HAL_GPIO_ReadPin(ENC_STEP_S1_GPIO_Port, ENC_STEP_S1_Pin);
    GPIO_PinState s2 = HAL_GPIO_ReadPin(ENC_STEP_S2_GPIO_Port, ENC_STEP_S2_Pin);
    bool a = (s1 == GPIO_PIN_RESET);
    bool b = (s2 == GPIO_PIN_RESET);

    /* Ліво = 0.001, центр = 0.01, право = 0.1 мм */
    if (a && !b) return STEP_SCALE_0_001;
    if (!a && !b) return STEP_SCALE_0_01;
    if (!a && b) return STEP_SCALE_0_1;
    return STEP_SCALE_0_01;
}

