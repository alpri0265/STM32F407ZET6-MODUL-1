#include "app.h"
#include "system_state.h"
#include "events.h"
#include "screens.h"
#include "bringup_config.h"
#if BRINGUP_MODE
#include "encoder_menu.h"
#include "menu.h"
#else
#include "system_config.h"
#include "planner.h"
#include "safety.h"
#endif

void app_init(void)
{
    system_state_init();
#if !BRINGUP_MODE
    system_config_init();
#endif
    events_init();
#if BRINGUP_MODE
    menu_init();
    encoder_menu_init();
#else
    planner_init();
    safety_init();
#endif
    screens_init();
}

void app_loop(void)
{
    events_process();
    system_state_process();
#if !BRINGUP_MODE
    safety_process();
    planner_process();
#endif
    screens_process();
}
