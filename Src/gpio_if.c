#include "gpio_if.h"
#include "board.h"

void gpio_if_init(void) {}

bool gpio_if_estop(void)
{
    return HAL_GPIO_ReadPin(ESTOP_PORT, ESTOP_PIN) == GPIO_PIN_RESET;
}
