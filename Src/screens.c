#include "screens.h"
#include "lcd.h"
#include "system_state.h"
#include "menu.h"
#include "encoder_menu.h"
#include "bringup_config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MENU_LINE_PREFIX "  "
#define MENU_LINE_SEL    "> "

static bool menu_need_redraw;

static void render_menu(void)
{
    lcd_clear();
    unsigned int n = menu_get_count();
    unsigned int sel = menu_get_selected();
    unsigned int i;
    char buf[22];
    const char *p;
    const char *prefix;

    for (i = 0; i < 4 && i < n; i++) {
        prefix = (i == sel) ? MENU_LINE_SEL : MENU_LINE_PREFIX;
        p = menu_get_item_text(i);
        (void)snprintf(buf, sizeof(buf), "%s%-16s", prefix, p);
        buf[sizeof(buf) - 1] = '\0';
        lcd_print_line((uint8_t)i, buf);
    }
    for ( ; i < 4; i++)
        lcd_print_line((uint8_t)i, "                    ");
}

void screens_init(void)
{
    lcd_init();
#if BRINGUP_MODE
    lcd_clear();
#endif
}

void screens_process(void)
{
#if BRINGUP_MODE
    if (system_state_is(SYS_STATE_READY)) {
        encoder_menu_action_t act = encoder_menu_get_action();
        bool need_render = false;
        if (act == ENCODER_MENU_ACTION_CW) {
            menu_select_next();
            encoder_menu_clear_action();
            need_render = true;
        } else if (act == ENCODER_MENU_ACTION_CCW) {
            menu_select_prev();
            encoder_menu_clear_action();
            need_render = true;
        } else if (act == ENCODER_MENU_ACTION_ENTER) {
            menu_enter();
            encoder_menu_clear_action();
            need_render = true;
        }
        if (!menu_need_redraw) {
            need_render = true;
            menu_need_redraw = true;
        }
        if (need_render)
            render_menu();
    } else {
        menu_need_redraw = false;
        lcd_clear();
        lcd_print_line(0, "INIT...");
    }
#else
    switch (system_state_get()) {
        case SYS_STATE_INIT:  lcd_print_line(0, "INIT"); break;
        case SYS_STATE_READY: lcd_print_line(0, "READY"); break;
        case SYS_STATE_HOLD:  lcd_print_line(0, "HOLD"); break;
        case SYS_STATE_ERROR: lcd_print_line(0, "ERROR"); break;
    }
#endif
}
