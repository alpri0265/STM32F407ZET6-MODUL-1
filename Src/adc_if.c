#include "adc_if.h"
#include "stm32f4xx_hal.h"

extern ADC_HandleTypeDef hadc1;

static uint32_t adc_channel_map(uint8_t ch)
{
    switch (ch) {
        case 1: return ADC_CHANNEL_1;
        case 2: return ADC_CHANNEL_2;
        case 3: return ADC_CHANNEL_3;
        case 4: return ADC_CHANNEL_4;
        default: return ADC_CHANNEL_1;
    }
}

uint16_t adc_if_read(uint8_t logical_channel)
{
    ADC_ChannelConfTypeDef cfg = {0};

    cfg.Channel      = adc_channel_map(logical_channel);
    cfg.Rank         = 1;
    cfg.SamplingTime = ADC_SAMPLETIME_15CYCLES;

    HAL_ADC_ConfigChannel(&hadc1, &cfg);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);

    return HAL_ADC_GetValue(&hadc1);
}
