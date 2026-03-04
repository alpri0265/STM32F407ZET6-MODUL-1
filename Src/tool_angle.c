#include "tool_angle.h"
#include "adc_if.h"
#include "board.h"

/* Зберігаємо опорну точку: raw при встановленні + бажаний кут у градусах (без обчислення offset) */
static uint16_t s_ref_raw;
static float s_ref_deg;

float tool_angle_get_deg(void)
{
    uint16_t raw = adc_if_read(ADC_CH_TOOL_ANGLE);
    int32_t diff = (int32_t)raw - (int32_t)s_ref_raw;
    float deg = s_ref_deg + (float)diff * 360.0f / 4095.0f;
    while (deg < 0.0f)   deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return deg;
}

void tool_angle_zero(void)
{
    s_ref_raw = adc_if_read(ADC_CH_TOOL_ANGLE);
    s_ref_deg = 0.0f;
}

void tool_angle_set_displayed_deg(float deg)
{
    s_ref_raw = adc_if_read(ADC_CH_TOOL_ANGLE);
    s_ref_deg = deg;
}
