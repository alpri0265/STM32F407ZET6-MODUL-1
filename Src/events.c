#include "events.h"
#include "bringup_config.h"
#include "gpio_if.h"
#include "system_state.h"
#if BRINGUP_MODE
#include "encoder_menu.h"
#else
#include "buttons.h"
#endif

void events_init(void)
{
#if !BRINGUP_MODE
    buttons_init();
#endif
}

void events_process(void)
{
#if BRINGUP_MODE
    encoder_menu_process();
#else
    buttons_process();
#endif
    if (gpio_if_estop())
        system_request_error(1);
#if !BRINGUP_MODE
    if (buttons_hold())  system_request_hold();
    if (buttons_resume()) system_request_resume();
    if (buttons_reset()) system_request_reset();
#endif
}
