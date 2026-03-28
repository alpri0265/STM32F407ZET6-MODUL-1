#include "gpio_if.h"
#include "board.h"
#include "main.h"

void gpio_if_init(void) {}

bool gpio_if_estop(void)
{
    GPIO_PinState s = HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin);
#if ESTOP_ACTIVE_HIGH
    return (s == GPIO_PIN_SET);
#else
    return (s == GPIO_PIN_RESET);
#endif
}
