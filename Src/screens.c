#include "screens.h"
#include "lcd.h"
#include "system_state.h"
#include "menu.h"
#include "encoder_menu.h"
#include "bringup_config.h"
#include "main.h"
#include "system_config.h"
#include "fault.h"
#include "axis_feedback.h"
#include "temperature.h"
#include "tool_angle.h"
#include "board.h"
#include "adc_if.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MENU_LINE_PREFIX "  "
#define MENU_LINE_SEL    "> "
#define LINE_LEN         20

static bool menu_need_redraw;

static void render_list_screen(void)
{
    lcd_clear();
    unsigned int n = menu_get_count();
    unsigned int sel = menu_get_selected();
    /* Прокрутка: якщо пунктів > 4, показуємо вікно з 4 рядків так, щоб курсор був видно */
    unsigned int first = 0u;
    if (n > 4u) {
        first = (sel >= 3u) ? (sel - 3u) : 0u;
        if (first + 4u > n) first = n - 4u;
    }
    char buf[LINE_LEN + 4];
    for (unsigned int i = 0; i < 4u; i++) {
        unsigned int idx = first + i;
        if (idx < n) {
            const char *prefix = (idx == sel) ? MENU_LINE_SEL : MENU_LINE_PREFIX;
            const char *p = menu_get_item_text(idx);
            (void)snprintf(buf, sizeof(buf), "%s%-16s", prefix, p);
        } else
            (void)snprintf(buf, sizeof(buf), "                    ");
        buf[LINE_LEN] = '\0';
        lcd_print_line((uint8_t)i, buf);
    }
}

static void render_info_jog(void)
{
    float x_mm = axis_feedback_pos_mm(AXIS_X);
    float z_mm = axis_feedback_pos_mm(AXIS_Z);
    char buf[LINE_LEN + 2];
    lcd_print_line(0, "Jog - use joystick");
    (void)snprintf(buf, sizeof(buf), "X: %.2f mm", (double)x_mm);
    lcd_print_line(1, buf);
    (void)snprintf(buf, sizeof(buf), "Z: %.2f mm", (double)z_mm);
    lcd_print_line(2, buf);
    lcd_print_line(3, "[Back]             ");
}

static void render_info_axis_x(void)
{
    const axis_cfg_t *c = system_axis_cfg(AXIS_X);
    char buf[LINE_LEN + 2];
    lcd_print_line(0, "Axis X             ");
    (void)snprintf(buf, sizeof(buf), "steps/mm: %.0f    ", (double)c->steps_per_mm);
    lcd_print_line(1, buf);
    (void)snprintf(buf, sizeof(buf), "max feed: %.0f   ", (double)c->max_feed);
    lcd_print_line(2, buf);
    (void)snprintf(buf, sizeof(buf), "min %.0f max %.0f ", (double)c->min_mm, (double)c->max_mm);
    lcd_print_line(3, buf);
}

static void render_info_axis_z(void)
{
    const axis_cfg_t *c = system_axis_cfg(AXIS_Z);
    char buf[LINE_LEN + 2];
    lcd_print_line(0, "Axis Z             ");
    (void)snprintf(buf, sizeof(buf), "steps/mm: %.0f    ", (double)c->steps_per_mm);
    lcd_print_line(1, buf);
    (void)snprintf(buf, sizeof(buf), "max feed: %.0f   ", (double)c->max_feed);
    lcd_print_line(2, buf);
    (void)snprintf(buf, sizeof(buf), "min %.0f max %.0f ", (double)c->min_mm, (double)c->max_mm);
    lcd_print_line(3, buf);
}

static void render_info_spindle(void)
{
    lcd_print_line(0, "Spindle settings   ");
    lcd_print_line(1, "PWM / RPM          ");
    lcd_print_line(2, "                   ");
    lcd_print_line(3, "[Back]             ");
}

static bool limits_read_x_neg(void) { return HAL_GPIO_ReadPin(LIM_X_NEG_GPIO_Port, LIM_X_NEG_Pin) == GPIO_PIN_SET; }
static bool limits_read_x_pos(void) { return HAL_GPIO_ReadPin(LIM_X_POS_GPIO_Port, LIM_X_POS_Pin) == GPIO_PIN_SET; }
static bool limits_read_z_neg(void) { return HAL_GPIO_ReadPin(LIM_Z_NEG_GPIO_Port, LIM_Z_NEG_Pin) == GPIO_PIN_SET; }
static bool limits_read_z_pos(void) { return HAL_GPIO_ReadPin(LIM_Z_POS_GPIO_Port, LIM_Z_POS_Pin) == GPIO_PIN_SET; }

static void render_info_limits(void)
{
    char buf[LINE_LEN + 2];
    lcd_print_line(0, "Limits status      ");
    lcd_print_line(1, "X-  X+  Z-  Z+     ");
    (void)snprintf(buf, sizeof(buf), " %d   %d   %d   %d       ",
        limits_read_x_neg() ? 1 : 0, limits_read_x_pos() ? 1 : 0,
        limits_read_z_neg() ? 1 : 0, limits_read_z_pos() ? 1 : 0);
    lcd_print_line(2, buf);
    lcd_print_line(3, "[Back]             ");
}

static void render_info_i2c_lcd(void)
{
    lcd_print_line(0, "I2C/LCD test       ");
    lcd_print_line(1, "OK                 ");
    lcd_print_line(2, "addr 0x27          ");
    lcd_print_line(3, "[Back]             ");
}

static void render_info_encoders(void)
{
    float x_mm = axis_feedback_pos_mm(AXIS_X);
    float z_mm = axis_feedback_pos_mm(AXIS_Z);
    char buf[LINE_LEN + 2];
    lcd_print_line(0, "Encoders X / Z     ");
    (void)snprintf(buf, sizeof(buf), "X: %.2f mm       ", (double)x_mm);
    lcd_print_line(1, buf);
    (void)snprintf(buf, sizeof(buf), "Z: %.2f mm       ", (double)z_mm);
    lcd_print_line(2, buf);
    lcd_print_line(3, "[Back]             ");
}

static void render_info_adc_fault(void)
{
    float t_c = temperature_get_c();
    uint16_t feed_raw = adc_if_read(ADC_CH_FEED_OVERRIDE);
    uint16_t enc_raw = adc_if_read(ADC_CH_TOOL_ANGLE);
    unsigned int enc_deg = (unsigned int)enc_raw * 360u / 4095u;
    uint16_t fault = fault_get();
    char buf[LINE_LEN + 4];
    lcd_print_line(0, "ADC / Fault            ");
    (void)snprintf(buf, sizeof(buf), "T:%.1f C  Feed:%u     ", (double)t_c, (unsigned)feed_raw);
    buf[LINE_LEN] = '\0';
    lcd_print_line(1, buf);
    (void)snprintf(buf, sizeof(buf), "Enc: %u deg           ", enc_deg);
    buf[LINE_LEN] = '\0';
    lcd_print_line(2, buf);
    (void)snprintf(buf, sizeof(buf), "Fault: %u  [Back]    ", (unsigned)fault);
    buf[LINE_LEN] = '\0';
    lcd_print_line(3, buf);
}

static const char* state_str(void)
{
    switch (system_state_get()) {
        case SYS_STATE_INIT:  return "INIT ";
        case SYS_STATE_READY: return "READY";
        case SYS_STATE_HOLD:  return "HOLD ";
        case SYS_STATE_ERROR: return "ERROR";
        default: return "?    ";
    }
}

static void render_info_info(void)
{
    char buf[LINE_LEN + 2];
    (void)snprintf(buf, sizeof(buf), "State: %s        ", state_str());
    lcd_print_line(0, buf);
    (void)snprintf(buf, sizeof(buf), "Fault: %u        ", (unsigned)fault_get());
    lcd_print_line(1, buf);
    lcd_print_line(2, "Build bringup      ");
    lcd_print_line(3, "[Back]             ");
}

static void render_info_screen(menu_screen_id_t id)
{
    switch (id) {
        case SCREEN_JOG:       render_info_jog();       break;
        case SCREEN_AXIS_X:    render_info_axis_x();    break;
        case SCREEN_AXIS_Z:    render_info_axis_z();    break;
        case SCREEN_SPINDLE:   render_info_spindle();   break;
        case SCREEN_I2C_LCD:   render_info_i2c_lcd();   break;
        case SCREEN_ENCODERS:  render_info_encoders();  break;
        case SCREEN_LIMITS:    render_info_limits();    break;
        case SCREEN_ADC_FAULT: render_info_adc_fault();  break;
        case SCREEN_INFO:      render_info_info();      break;
        default:
            lcd_print_line(0, "Unknown screen     ");
            lcd_print_line(1, "                  ");
            lcd_print_line(2, "                  ");
            lcd_print_line(3, "[Back]             ");
            break;
    }
}

static void render_current_screen(void)
{
    menu_screen_id_t cur = menu_current_screen();
    if (menu_screen_type(cur) == MENU_SCREEN_LIST)
        render_list_screen();
    else
        render_info_screen(cur);
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
        menu_screen_id_t cur = menu_current_screen();
        menu_screen_type_t st = menu_screen_type(cur);

        if (act != ENCODER_MENU_ACTION_NONE) {
            if (st == MENU_SCREEN_INFO && menu_can_back()) {
                menu_back();
                need_render = true;
            } else if (st == MENU_SCREEN_LIST) {
                if (act == ENCODER_MENU_ACTION_CW) {
                    menu_select_next();
                    need_render = true;
                } else if (act == ENCODER_MENU_ACTION_CCW) {
                    menu_select_prev();
                    need_render = true;
                } else if (act == ENCODER_MENU_ACTION_ENTER) {
                    menu_enter();
                    need_render = true;
                }
            }
            /* Завжди скидаємо дію, щоб encoder_menu_process() знову опитував кнопки */
            encoder_menu_clear_action();
        }

        if (!menu_need_redraw) {
            need_render = true;
            menu_need_redraw = true;
        }
        /* Екран ADC/Fault показує живі значення — завжди перемальовувати, щоб tick і ADC оновлювалися */
        if (cur == SCREEN_ADC_FAULT)
            need_render = true;
        if (need_render)
            render_current_screen();
    } else {
        menu_need_redraw = false;
        lcd_clear();
        lcd_print_line(0, "INIT...            ");
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
