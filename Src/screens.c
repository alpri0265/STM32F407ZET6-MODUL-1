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
#include "jog.h"
#include "sl_limits.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MENU_LINE_PREFIX "  "
#define MENU_LINE_SEL    "> "
#define LINE_LEN         20

static bool menu_need_redraw;

/* Режим введення куту вручну на екрані Tool angle; value в десятих (0-3599), cursor 0-3 (сотні, десятки, одиниці, десяті) */
static bool tool_angle_edit_mode;
static unsigned int tool_angle_edit_value;
static unsigned int tool_angle_edit_cursor;

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
    float x_mm = 0.0f, z_mm = 0.0f;
    jog_get_pos_mm(&x_mm, &z_mm);
    int32_t xf = axis_feedback_pos_um(AXIS_X);
    int32_t zf = axis_feedback_pos_um(AXIS_Z);
    unsigned int ju, jd, jl, jr, rapid;
    uint32_t steps_x, steps_z;
    jog_get_joy_state(&ju, &jd, &jl, &jr);
    jog_get_step_counts(&steps_x, &steps_z);
    jog_get_rapid_state(&rapid);
    char buf[LINE_LEN + 2];
    {
        int32_t x_abs = (xf < 0) ? -xf : xf;
        int32_t z_abs = (zf < 0) ? -zf : zf;
        char tmp[48];
        (void)snprintf(tmp, sizeof(tmp), "Xf:%c%ld.%03ld",
                       (xf < 0) ? '-' : '+', (long)(x_abs / 1000), (long)(x_abs % 1000));
        (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp); /* pad/trim to clear old chars */
        buf[LINE_LEN] = '\0';
        lcd_print_line(0, buf);

        (void)snprintf(tmp, sizeof(tmp), "Zf:%c%ld.%03ld",
                       (zf < 0) ? '-' : '+', (long)(z_abs / 1000), (long)(z_abs % 1000));
        (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
        buf[LINE_LEN] = '\0';
        lcd_print_line(1, buf);
    }
    /* Рядок 3 (індекс 2): короткі мітки, щоб влізло в 20 символів */
    (void)snprintf(buf, sizeof(buf), "X:%lu Z:%lu",
                   (unsigned long)steps_x, (unsigned long)steps_z);
    buf[LINE_LEN] = '\0';
    lcd_print_line(2, buf);
    (void)snprintf(buf, sizeof(buf), "U%u D%u L%u R%u *%u Up=Bk", ju, jd, jl, jr, rapid);
    buf[LINE_LEN] = '\0';
    lcd_print_line(3, buf);
}

static void render_info_feed_manual(void)
{
    uint16_t feed_raw = adc_if_read(ADC_CH_FEED_OVERRIDE);
    unsigned int feed_pct = 30u + ((unsigned)feed_raw * 120u) / 4095u;
    char buf[LINE_LEN + 2];
    lcd_print_line(0, "Manual feed        ");
    (void)snprintf(buf, sizeof(buf), "Pot: %u%%           ", feed_pct);
    buf[LINE_LEN] = '\0';
    lcd_print_line(1, buf);
    lcd_print_line(2, "PA4 potentiometer  ");
    lcd_print_line(3, "[Back]             ");
}

static void render_info_feed_auto(void)
{
    float x_mm = 0.0f, z_mm = 0.0f;
    jog_get_pos_mm(&x_mm, &z_mm);
    int32_t xf = axis_feedback_pos_um(AXIS_X);
    int32_t zf = axis_feedback_pos_um(AXIS_Z);
    bool x_ok = sl_limits_x_taught();
    bool z_ok = sl_limits_z_taught();
    char buf[LINE_LEN + 2];
    lcd_print_line(0, "Feed Auto        ");
    {
        int32_t x_abs = (xf < 0) ? -xf : xf;
        int32_t z_abs = (zf < 0) ? -zf : zf;
        char tmp[48];
        (void)snprintf(tmp, sizeof(tmp), "Xf:%c%ld.%03ld",
                       (xf < 0) ? '-' : '+', (long)(x_abs / 1000), (long)(x_abs % 1000));
        (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
        buf[LINE_LEN] = '\0';
        lcd_print_line(1, buf);

        (void)snprintf(tmp, sizeof(tmp), "Zf:%c%ld.%03ld",
                       (zf < 0) ? '-' : '+', (long)(z_abs / 1000), (long)(z_abs % 1000));
        (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
        buf[LINE_LEN] = '\0';
        lcd_print_line(2, buf);
    }
    (void)snprintf(buf, sizeof(buf), "X%c Z%c Joy SL [Bk]",
                   x_ok ? '+' : '-', z_ok ? '+' : '-');
    buf[LINE_LEN] = '\0';
    lcd_print_line(3, buf);
}

static void render_info_z_passes(void)
{
    unsigned int n = jog_get_z_passes();
    char buf[LINE_LEN + 2];
    lcd_print_line(0, "Z: passes (axis Z) ");
    (void)snprintf(buf, sizeof(buf), "Passes: %u         ", n);
    buf[LINE_LEN] = '\0';
    lcd_print_line(1, buf);
    lcd_print_line(2, "0=unlimited        ");
    lcd_print_line(3, "Dn+ Up- [Ent]=Back ");
}

/* Фокус на екрані X passes: 0 = Passes, 1 = X- depth */
static unsigned int s_x_passes_focus = 1u;
static menu_screen_id_t s_prev_menu_screen = (menu_screen_id_t)-1;

/* X: проходів по X (ліміт) та глибина X- (0.01..0.30 mm) після кожного проходу Z.
 * Значення виводимо цілими (0.%02u), щоб не залежати від _printf_float. */
static void render_info_x_passes(void)
{
    unsigned int passes = jog_get_x_passes();
    unsigned int step_i = jog_get_x_step_index();
    unsigned int step_val = step_i + 1u;  /* 1..30 → 0.01..0.30 */
    char buf[LINE_LEN + 2];
    const char *cur0 = (s_x_passes_focus == 0u) ? ">" : " ";
    const char *cur1 = (s_x_passes_focus == 1u) ? ">" : " ";
    lcd_print_line(0, "X passes & depth  ");
    (void)snprintf(buf, sizeof(buf), "%sPasses: %u        ", cur0, passes);
    buf[LINE_LEN] = '\0';
    lcd_print_line(1, buf);
    (void)snprintf(buf, sizeof(buf), "%s X- 0.%02u mm     ", cur1, step_val);
    buf[LINE_LEN] = '\0';
    lcd_print_line(2, buf);
    lcd_print_line(3, "Dn+ Up- [Ent]=row");
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

/* Усі кінцевики — індуктивні NPN: спрацювання = LOW (вихід сенсора в GND) */
static bool limits_read_x_neg(void) { return HAL_GPIO_ReadPin(LIM_X_NEG_GPIO_Port, LIM_X_NEG_Pin) == GPIO_PIN_RESET; }
static bool limits_read_x_pos(void) { return HAL_GPIO_ReadPin(LIM_X_POS_GPIO_Port, LIM_X_POS_Pin) == GPIO_PIN_RESET; }
static bool limits_read_z_neg(void) { return HAL_GPIO_ReadPin(LIM_Z_NEG_GPIO_Port, LIM_Z_NEG_Pin) == GPIO_PIN_RESET; }
static bool limits_read_z_pos(void) { return HAL_GPIO_ReadPin(LIM_Z_POS_GPIO_Port, LIM_Z_POS_Pin) == GPIO_PIN_RESET; }

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

/* Тест кнопок та LED програмних лімітів (SL). На екрані — стан кнопок; LED світяться при натисканні. */
static void render_info_sl_test(void)
{
    unsigned int xn = sl_limits_btn_x_neg() ? 1u : 0u;
    unsigned int xp = sl_limits_btn_x_pos() ? 1u : 0u;
    unsigned int zn = sl_limits_btn_z_neg() ? 1u : 0u;
    unsigned int zp = sl_limits_btn_z_pos() ? 1u : 0u;
    char buf[LINE_LEN + 2];
    lcd_print_line(0, "SL buttons & LEDs   ");
    lcd_print_line(1, "X-   X+   Z-   Z+  ");
    (void)snprintf(buf, sizeof(buf), " %u    %u    %u    %u   ", xn, xp, zn, zp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(2, buf);
    lcd_print_line(3, "Press=LED on [Back]");
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
    float enc_deg_f = (float)enc_raw * 360.0f / 4095.0f;
    unsigned int enc_d = (unsigned int)enc_deg_f;
    unsigned int enc_t = (unsigned int)(enc_deg_f * 10.0f) % 10u;
    uint16_t fault = fault_get();
    char buf[LINE_LEN + 4];
    unsigned int tc_d = (unsigned int)(t_c < 0.0f ? -t_c : t_c);
    unsigned int tc_t = (unsigned int)((t_c < 0.0f ? -t_c : t_c) * 10.0f) % 10u;
    lcd_print_line(0, "ADC / Fault            ");
    (void)snprintf(buf, sizeof(buf), "T:%s%u.%u C Feed:%u   ", t_c < 0.0f ? "-" : "", tc_d, tc_t, (unsigned)feed_raw);
    buf[LINE_LEN] = '\0';
    lcd_print_line(1, buf);
    (void)snprintf(buf, sizeof(buf), "Enc: %u.%u %c          ", enc_d, enc_t, 0xFF);
    buf[LINE_LEN] = '\0';
    lcd_print_line_deg(2, buf);
    (void)snprintf(buf, sizeof(buf), "Fault: %u [Back]    ", (unsigned)fault);
    buf[LINE_LEN] = '\0';
    lcd_print_line(3, buf);
}

static void render_info_tool_angle_calib(void)
{
    float deg = tool_angle_get_deg();
    unsigned int d = (unsigned int)deg;
    unsigned int t = (unsigned int)(deg * 10.0f) % 10u;
    char buf[LINE_LEN + 4];
    lcd_print_line(0, "Calibrate encoder   ");
    lcd_print_line(1, "Set to 180 deg then ");
    lcd_print_line(2, "Enter=Cal Up=Back   ");
    (void)snprintf(buf, sizeof(buf), "  Now: %u.%u %c      ", d, t, 0xFF);
    buf[LINE_LEN] = '\0';
    lcd_print_line_deg(3, buf);
}

static void render_info_tool_angle(void)
{
    char buf[LINE_LEN + 4];
    if (tool_angle_edit_mode) {
        unsigned int v = tool_angle_edit_value;
        unsigned int h = (v / 10u) / 100u;   /* сотні 0-3 */
        unsigned int t = (v / 10u) / 10u % 10u;
        unsigned int o = (v / 10u) % 10u;
        unsigned int d = v % 10u;             /* десяті */
        (void)snprintf(buf, sizeof(buf), "Set angle (%c)        ", 0xFF);
        buf[LINE_LEN] = '\0';
        lcd_print_line_deg(0, buf);
        (void)snprintf(buf, sizeof(buf), "Set: %u%u%u.%u%c     ", h, t, o, d, 0xFF);
        buf[LINE_LEN] = '\0';
        lcd_print_line_deg(1, buf);
        /* Курсор під розрядом: "Set: " = 5 символів, потім цифри 5,6,7, крапка 8, цифра 9 */
        { char cur[LINE_LEN + 1]; unsigned int i, pos = (tool_angle_edit_cursor < 3u) ? (5u + tool_angle_edit_cursor) : 9u;
          for (i = 0u; i < LINE_LEN; i++) cur[i] = (i == pos) ? '^' : ' ';
          cur[LINE_LEN] = '\0';
          lcd_print_line(2, cur); }
        lcd_print_line(3, "Enter=next Up/Down=+/- ");
    } else {
        float deg = tool_angle_get_deg();
        unsigned int d = (unsigned int)deg;
        unsigned int t = (unsigned int)(deg * 10.0f) % 10u;
        lcd_print_line(0, "  Kut instrumentu      ");
        (void)snprintf(buf, sizeof(buf), "  %u.%u %c            ", d, t, 0xFF);
        buf[LINE_LEN] = '\0';
        lcd_print_line_deg(1, buf);
        lcd_print_line(2, "Enter=Zero Down=Set    ");
        lcd_print_line(3, "Up=Back               ");
    }
}

/* Оновлює тільки рядок з кутом — без перемальовування всього екрану, щоб не мерехтіло */
static void tool_angle_refresh_value_only(void)
{
    float deg = tool_angle_get_deg();
    unsigned int d = (unsigned int)deg;
    unsigned int t = (unsigned int)(deg * 10.0f) % 10u;
    char buf[LINE_LEN + 4];
    (void)snprintf(buf, sizeof(buf), "  %u.%u %c            ", d, t, 0xFF);
    buf[LINE_LEN] = '\0';
    lcd_print_line_deg(1, buf);
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
        case SCREEN_FEED_MANUAL: render_info_feed_manual(); break;
        case SCREEN_FEED_AUTO:   render_info_feed_auto();   break;
        case SCREEN_Z_PASSES:   render_info_z_passes();    break;
        case SCREEN_X_PASSES:   render_info_x_passes();    break;
        case SCREEN_AXIS_X:    render_info_axis_x();    break;
        case SCREEN_AXIS_Z:    render_info_axis_z();    break;
        case SCREEN_SPINDLE:   render_info_spindle();   break;
        case SCREEN_I2C_LCD:   render_info_i2c_lcd();   break;
        case SCREEN_ENCODERS:  render_info_encoders();  break;
        case SCREEN_LIMITS:    render_info_limits();    break;
        case SCREEN_SL_TEST:   render_info_sl_test();   break;
        case SCREEN_ADC_FAULT: render_info_adc_fault();  break;
        case SCREEN_TOOL_ANGLE:     render_info_tool_angle();     break;
        case SCREEN_TOOL_ANGLE_CALIB: render_info_tool_angle_calib(); break;
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

        if (cur == SCREEN_X_PASSES && s_prev_menu_screen != SCREEN_X_PASSES)
            s_x_passes_focus = 1u;

        if (cur != SCREEN_TOOL_ANGLE)
            tool_angle_edit_mode = false;

        if (act != ENCODER_MENU_ACTION_NONE) {
            if (cur == SCREEN_TOOL_ANGLE && menu_can_back()) {
                if (tool_angle_edit_mode) {
                    if (act == ENCODER_MENU_ACTION_ENTER) {
                        if (tool_angle_edit_cursor < 3u) {
                            tool_angle_edit_cursor++;
                            need_render = true;
                        } else {
                            tool_angle_set_displayed_deg((float)tool_angle_edit_value / 10.0f);
                            tool_angle_edit_mode = false;
                            need_render = true;
                        }
                    } else if (act == ENCODER_MENU_ACTION_CCW || act == ENCODER_MENU_ACTION_CW) {
                        static const unsigned int place[] = { 1000u, 100u, 10u, 1u };
                        unsigned int step = place[tool_angle_edit_cursor];
                        if (act == ENCODER_MENU_ACTION_CCW) {
                            tool_angle_edit_value += step;
                            if (tool_angle_edit_value > 3599u) tool_angle_edit_value = 3599u;
                        } else {
                            if (tool_angle_edit_value >= step) tool_angle_edit_value -= step;
                            else tool_angle_edit_value = 0u;
                        }
                        need_render = true;
                    }
                } else {
                    if (act == ENCODER_MENU_ACTION_ENTER) {
                        tool_angle_zero();
                        need_render = true;
                    } else if (act == ENCODER_MENU_ACTION_CW) {
                        tool_angle_edit_mode = true;
                        tool_angle_edit_cursor = 0u;
                        {
                            float d = tool_angle_get_deg();
                            if (d < 0.0f) d = 0.0f;
                            if (d >= 360.0f) d = 0.0f;
                            tool_angle_edit_value = (unsigned int)(d * 10.0f + 0.5f);
                            if (tool_angle_edit_value >= 3600u) tool_angle_edit_value = 0u;
                        }
                        need_render = true;
                    } else {
                        menu_back();
                        need_render = true;
                    }
                }
            } else if (cur == SCREEN_TOOL_ANGLE_CALIB && menu_can_back()) {
                if (act == ENCODER_MENU_ACTION_ENTER) {
                    tool_angle_calibrate_180();
                    need_render = true;
                } else if (act == ENCODER_MENU_ACTION_CCW) {
                    menu_back();
                    need_render = true;
                }
            } else if (cur == SCREEN_Z_PASSES && menu_can_back()) {
                unsigned int v = jog_get_z_passes();
                if (act == ENCODER_MENU_ACTION_ENTER) {
                    menu_back();
                    need_render = true;
                } else if (act == ENCODER_MENU_ACTION_CCW) {
                    if (v == 0u)
                        menu_back();
                    else
                        jog_set_z_passes(v - 1u);
                    need_render = true;
                } else if (act == ENCODER_MENU_ACTION_CW) {
                    jog_set_z_passes(v < 99u ? v + 1u : 99u);
                    need_render = true;
                }
            } else if (cur == SCREEN_X_PASSES && menu_can_back()) {
                unsigned int passes = jog_get_x_passes();
                unsigned int step_i = jog_get_x_step_index();
                if (act == ENCODER_MENU_ACTION_ENTER) {
                    if (s_x_passes_focus == 0u) {
                        s_x_passes_focus = 1u;
                    } else {
                        s_x_passes_focus = 0u;
                        menu_back();
                    }
                    need_render = true;
                } else if (act == ENCODER_MENU_ACTION_CCW) {
                    if (s_x_passes_focus == 0u) {
                        if (passes == 0u)
                            menu_back();
                        else
                            jog_set_x_passes(passes - 1u);
                    } else {
                        if (step_i > 0u) jog_set_x_step_index(step_i - 1u);
                    }
                    need_render = true;
                } else if (act == ENCODER_MENU_ACTION_CW) {
                    if (s_x_passes_focus == 0u) {
                        jog_set_x_passes(passes < 99u ? passes + 1u : 99u);
                    } else {
                        jog_set_x_step_index(step_i < 29u ? step_i + 1u : 29u);
                    }
                    need_render = true;
                }
            } else if ((cur == SCREEN_JOG || cur == SCREEN_FEED_AUTO || cur == SCREEN_SL_TEST) && menu_can_back()) {
                if (act == ENCODER_MENU_ACTION_CCW) {
                    /* Зберігаємо останній показ Xf/Zf при виході з Jog/Feed Auto */
                    if (cur == SCREEN_JOG || cur == SCREEN_FEED_AUTO)
                        axis_feedback_save_last_displayed();
                    menu_back();
                    need_render = true;
                }
                /* Enter і CW — для Jog (перемикання осі, рух), не виходимо */
            } else if (st == MENU_SCREEN_INFO && menu_can_back()) {
                if (cur == SCREEN_ENCODERS)
                    axis_feedback_save_last_displayed();
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
        /* ADC/Fault — повне оновлення не частіше раз на 250 мс */
        /* Limits — оновлення кожні 150 мс, щоб кінцевики відображалися в реальному часі */
        {
            static uint32_t last_adc_tick;
            static uint32_t last_limits_tick;
            static uint32_t last_jog_tick;
            static uint32_t last_linenc_save_tick;
            static int32_t  last_linenc_save_x_um;
            static int32_t  last_linenc_save_z_um;
            uint32_t now = HAL_GetTick();
            if (cur == SCREEN_ADC_FAULT) {
                if ((now - last_adc_tick) >= 250u)
                    need_render = true;
                if (need_render)
                    last_adc_tick = now;
            }
            if (cur == SCREEN_LIMITS || cur == SCREEN_SL_TEST) {
                if ((now - last_limits_tick) >= 150u)
                    need_render = true;
                if (need_render)
                    last_limits_tick = now;
            }
            if (cur == SCREEN_JOG || cur == SCREEN_FEED_AUTO) {
                if ((now - last_jog_tick) >= 100u)
                    need_render = true;
                if (need_render)
                    last_jog_tick = now;

                /* Автозбереження раз в період, щоб гарантовано пережити 15с до power-off.
                 * Це стирає flash досить часто (для тесту ок), але працює надійно.
                 */
#define LINENC_AUTOSAVE_PERIOD_MS  5000u
                if ((now - last_linenc_save_tick) >= LINENC_AUTOSAVE_PERIOD_MS) {
                    int32_t x_um = axis_feedback_pos_um(AXIS_X);
                    int32_t z_um = axis_feedback_pos_um(AXIS_Z);
                    if (x_um != last_linenc_save_x_um || z_um != last_linenc_save_z_um) {
                        axis_feedback_save_last_displayed();
                        last_linenc_save_tick = now;
                        last_linenc_save_x_um = x_um;
                        last_linenc_save_z_um = z_um;
                    }
                }
            }
            if (cur == SCREEN_FEED_MANUAL) {
                if ((now - last_adc_tick) >= 250u)
                    need_render = true;
                if (need_render)
                    last_adc_tick = now;
            }
        }
        if (need_render)
            render_current_screen();
        s_prev_menu_screen = cur;
        /* Кут інструменту: тільки рядок з числом оновлювати раз на 100 мс (не в режимі Set) */
        if (cur == SCREEN_TOOL_ANGLE && !tool_angle_edit_mode) {
            static uint32_t last_tool_tick;
            uint32_t now = HAL_GetTick();
            if ((now - last_tool_tick) >= 100u) {
                tool_angle_refresh_value_only();
                last_tool_tick = now;
            }
        }
        /* Екран калібрування: оновлювати рядок "Now" кожні 100 мс */
        if (cur == SCREEN_TOOL_ANGLE_CALIB) {
            static uint32_t last_calib_tick;
            uint32_t now = HAL_GetTick();
            if ((now - last_calib_tick) >= 100u) {
                float deg = tool_angle_get_deg();
                unsigned int d = (unsigned int)deg;
                unsigned int t = (unsigned int)(deg * 10.0f) % 10u;
                char buf[LINE_LEN + 4];
                (void)snprintf(buf, sizeof(buf), "  Now: %u.%u %c      ", d, t, 0xFF);
                buf[LINE_LEN] = '\0';
                lcd_print_line_deg(3, buf);
                last_calib_tick = now;
            }
        }
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
