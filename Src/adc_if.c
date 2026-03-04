#include "adc_if.h"
#include "stm32f4xx_hal.h"

extern ADC_HandleTypeDef hadc1;

static uint32_t adc_channel_map(uint8_t ch)
{
    switch (ch) {
        case 0: return ADC_CHANNEL_0;  /* PA0 */
        case 1: return ADC_CHANNEL_1;  /* PA1 */
        case 2: return ADC_CHANNEL_2;  /* PA2 */
        case 3: return ADC_CHANNEL_3;  /* PA3 */
        case 4: return ADC_CHANNEL_4;  /* PA4 */
        case 5: return ADC_CHANNEL_5;  /* PA5 = tool angle (absolute encoder) */
        default: return ADC_CHANNEL_1;
    }
}

uint16_t adc_if_read(uint8_t logical_channel)
{
    ADC_ChannelConfTypeDef cfg = {0};

    cfg.Channel      = adc_channel_map(logical_channel);
    cfg.Rank         = 1;
    cfg.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    HAL_ADC_Stop(&hadc1);
    HAL_ADC_ConfigChannel(&hadc1, &cfg);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);

    return HAL_ADC_GetValue(&hadc1);
}
