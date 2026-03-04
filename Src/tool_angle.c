#include "tool_angle.h"
#include "adc_if.h"
#include "board.h"

float tool_angle_get_deg(void)
{
    uint16_t v = adc_if_read(ADC_CH_TOOL_ANGLE);
    return (float)v * 360.0f / 4095.0f;
}
