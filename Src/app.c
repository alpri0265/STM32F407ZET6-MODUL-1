#include "app.h"
#include "system_state.h"
#include "events.h"
#include "screens.h"
#include "bringup_config.h"
#include "system_config.h"
#include "tool_angle.h"
#if BRINGUP_MODE
#include "encoder_menu.h"
#include "menu.h"
#include "jog.h"
#include "board.h"
#include "adc_if.h"
#else
#include "planner.h"
#include "safety.h"
#endif

void app_init(void)
{
    system_state_init();
    system_config_init();
    tool_angle_init();  /* load saved ref from Flash */
    events_init();
#if BRINGUP_MODE
    menu_init();
    encoder_menu_init();
    jog_init();
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
#if BRINGUP_MODE
    if (menu_current_screen() == SCREEN_JOG || menu_current_screen() == SCREEN_FEED_MANUAL) {
        uint16_t f = adc_if_read(ADC_CH_FEED_OVERRIDE);
        jog_set_feed_override(f);
    }
    jog_process();
#endif
    screens_process();
}
