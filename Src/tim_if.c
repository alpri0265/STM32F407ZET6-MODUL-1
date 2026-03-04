#include "tim_if.h"
#include "stm32f4xx_hal.h"

extern TIM_HandleTypeDef htim1;

void tim_if_start(uint32_t hz)
{
    uint32_t clk = HAL_RCC_GetPCLK2Freq();
    uint32_t arr = (clk / hz) - 1;

    __HAL_TIM_SET_AUTORELOAD(&htim1, arr);
    __HAL_TIM_SET_COUNTER(&htim1, 0);

    HAL_TIM_Base_Start_IT(&htim1);
}

void tim_if_stop(void)
{
    HAL_TIM_Base_Stop_IT(&htim1);
}
