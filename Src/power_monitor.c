#include "power_monitor.h"
#include "adc_if.h"
#include "board.h"

int power_fault(void)
{
    uint16_t v = adc_if_read(ADC_CH_POWER_MONITOR);
    return (v > 3800);
}
