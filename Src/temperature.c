#include "temperature.h"
#include "adc_if.h"
#include "board.h"

float temperature_get_c(void)
{
    uint16_t v = adc_if_read(ADC_CH_TEMPERATURE);
    return (float)v * 100.0f / 4095.0f;
}
