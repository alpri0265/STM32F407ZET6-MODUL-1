#include "spindle.h"
#include "stm32f4xx_hal.h"

extern TIM_HandleTypeDef htim1;

void spindle_init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
}

void spindle_set_rpm(uint16_t rpm)
{
    uint32_t pwm = rpm; /* scale later */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm);
}

void spindle_stop(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
}
