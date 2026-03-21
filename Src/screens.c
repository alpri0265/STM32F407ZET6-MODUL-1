#include "screens.h"
#include "lcd.h"
#include "ili9341.h"
#include "system_state.h"
#include "menu.h"
#include "encoder_menu.h"
#include "touch.h"
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
/* Текст меню: OFFSET_X(8) + 20 символів * 12 px — не треба чистити всю ширину 320 */
#define MENU_LIST_FILL_W 248

#define LCD_OFFSET_Y     8
#define LCD_ROW_HEIGHT   24   /* як FONT_H+ROW_GAP у lcd.c */
#define TOUCH_COOLDOWN_MS 250

static bool menu_need_redraw;
static uint32_t touch_last_handled;
static bool s_list_cache_was_info;

/* Режим введення куту вручну на екрані Tool angle; value в десятих (0-3599), cursor 0-3 (сотні, десятки, одиниці, десяті) */
static bool tool_angle_edit_mode;
static unsigned int tool_angle_edit_value;
static unsigned int tool_angle_edit_cursor;

/* Mechanics editor state (SCREEN_MECHANICS). */
static uint8_t mech_edit_axis = 0u;   /* 0=X, 1=Z */
static uint8_t mech_edit_field = 0u;  /* 0=axis, 1=pitch, 2=steps/rev, 3=ratio */

/* Axis X/Z editor state (SCREEN_AXIS_X, SCREEN_AXIS_Z). */
static uint8_t axis_edit_field = 0u;  /* 0=max_feed, 1=min_mm, 2=max_mm */

static void render_list_screen(void)
{
    static char s_last_lines[6][LINE_LEN + 2];
    if (s_list_cache_was_info) {
        s_list_cache_was_info = false;
        for (int i = 0; i < 6; i++) s_last_lines[i][0] = '\0';
    }
    unsigned int n = menu_get_count();
    unsigned int sel = menu_get_selected();
    unsigned int first = 0u;
    if (n > 6u) {
        first = (sel >= 5u) ? (sel - 5u) : 0u;
        if (first + 6u > n) first = n - 6u;
    }
    char buf[LINE_LEN + 4];
    for (unsigned int i = 0; i < 6u; i++) {
        unsigned int idx = first + i;
        if (idx < n) {
            const char *prefix = (idx == sel) ? MENU_LINE_SEL : MENU_LINE_PREFIX;
            const char *p = menu_get_item_text(idx);
            (void)snprintf(buf, sizeof(buf), "%s%-16s", prefix, p);
        } else
            (void)snprintf(buf, sizeof(buf), "                    ");
        buf[LINE_LEN] = '\0';
        if (strcmp(s_last_lines[i], buf) != 0) {
            (void)strncpy(s_last_lines[i], buf, LINE_LEN);
            s_last_lines[i][LINE_LEN] = '\0';
            ili9341_fill_rect(0, 8 + i * LCD_ROW_HEIGHT, MENU_LIST_FILL_W, LCD_ROW_HEIGHT, 0x0000);
            lcd_print_line((uint8_t)i, buf);
        }
    }
}

static void render_info_mechanics(void)
{
    axis_id_t axis = (mech_edit_axis == 0u) ? AXIS_X : AXIS_Z;
    const axis_cfg_t *ax = system_axis_cfg(axis);

    uint16_t pitch_x100 = system_mech_get_pitch_x100(axis);
    uint16_t ms = system_mech_get_microstep(axis);
    uint32_t ratio_x1000 = system_mech_get_reducer_ratio_x1000(axis);

    unsigned int pitch_mm = pitch_x100 / 100u;
    unsigned int pitch_fr = pitch_x100 % 100u;
    unsigned int ratio_i = (unsigned int)(ratio_x1000 / 1000u);
    unsigned long spmm = (unsigned long)(ax->steps_per_mm + 0.5f);

    char buf[LINE_LEN + 2];
    char tmp[64];

    /* Line 0: axis */
    (void)snprintf(tmp, sizeof(tmp), "%cMech %c", (mech_edit_field == 0u) ? '>' : ' ',
                   (axis == AXIS_X) ? 'X' : 'Z');
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(0, buf);

    /* Line 1: pitch */
    (void)snprintf(tmp, sizeof(tmp), "%cPitch:%u.%02umm",
                   (mech_edit_field == 1u) ? '>' : ' ', pitch_mm, pitch_fr);
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(1, buf);

    /* Line 2: steps/rev */
    (void)snprintf(tmp, sizeof(tmp), "%cMicro:%u",
                   (mech_edit_field == 2u) ? '>' : ' ', (unsigned)ms);
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(2, buf);

    /* Line 3: integer reducer ratio (10:1 => R=10) + resulting steps/mm */
    (void)snprintf(tmp, sizeof(tmp), "%cR:%u sp:%lu",
                   (mech_edit_field == 3u) ? '>' : ' ', ratio_i, spmm);
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(3, buf);
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
    /* Рядок 2 — кроки; 3 — лише джойстик; 4 — підказка Back (окремо, щоб не злипалось) */
    (void)snprintf(buf, sizeof(buf), "X:%lu Z:%lu",
                   (unsigned long)steps_x, (unsigned long)steps_z);
    buf[LINE_LEN] = '\0';
    lcd_print_line(2, buf);
    (void)snprintf(buf, sizeof(buf), "U%u D%u L%u R%u *%u",
                   ju, jd, jl, jr, rapid);
    buf[LINE_LEN] = '\0';
    lcd_print_line(3, buf);
    lcd_print_line(4, "< Back              ");
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
    char tmp[48];
    unsigned long spmm = (unsigned long)(c->steps_per_mm + 0.5f);
    unsigned long mf = (unsigned long)(c->max_feed + 0.5f);
    int minm = (int)(c->min_mm);
    int maxm = (int)(c->max_mm);
    lcd_print_line(0, "Axis X             ");
    (void)snprintf(tmp, sizeof(tmp), "steps/mm: %lu     ", spmm);
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(1, buf);
    (void)snprintf(tmp, sizeof(tmp), "%cmax: %lu        ", (axis_edit_field == 0u) ? '>' : ' ', mf);
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(2, buf);
    if (axis_edit_field == 1u)
        (void)snprintf(tmp, sizeof(tmp), ">min %d max %d   ", minm, maxm);
    else if (axis_edit_field == 2u)
        (void)snprintf(tmp, sizeof(tmp), " min %d>max %d  ", minm, maxm);
    else
        (void)snprintf(tmp, sizeof(tmp), " min %d max %d   ", minm, maxm);
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(3, buf);
}

static void render_info_axis_z(void)
{
    const axis_cfg_t *c = system_axis_cfg(AXIS_Z);
    char buf[LINE_LEN + 2];
    char tmp[48];
    unsigned long spmm = (unsigned long)(c->steps_per_mm + 0.5f);
    unsigned long mf = (unsigned long)(c->max_feed + 0.5f);
    int minm = (int)(c->min_mm);
    int maxm = (int)(c->max_mm);
    lcd_print_line(0, "Axis Z             ");
    (void)snprintf(tmp, sizeof(tmp), "steps/mm: %lu     ", spmm);
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(1, buf);
    (void)snprintf(tmp, sizeof(tmp), "%cmax: %lu        ", (axis_edit_field == 0u) ? '>' : ' ', mf);
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(2, buf);
    if (axis_edit_field == 1u)
        (void)snprintf(tmp, sizeof(tmp), ">min %d max %d   ", minm, maxm);
    else if (axis_edit_field == 2u)
        (void)snprintf(tmp, sizeof(tmp), " min %d>max %d  ", minm, maxm);
    else
        (void)snprintf(tmp, sizeof(tmp), " min %d max %d   ", minm, maxm);
    (void)snprintf(buf, sizeof(buf), "%-20.20s", tmp);
    buf[LINE_LEN] = '\0';
    lcd_print_line(3, buf);
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
        case SCREEN_MECHANICS:  render_info_mechanics(); break;
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
    if (menu_screen_type(cur) == MENU_SCREEN_LIST) {
        /* Після інфо (Jog тощо) — очистити текстову зону; інакше лишаються 3–4 рядки Jog.
         * Кеш рядків скидається в render_list_screen() через s_list_cache_was_info (не обнуляти тут!). */
        if (menu_screen_type(s_prev_menu_screen) == MENU_SCREEN_INFO)
            lcd_clear_rows();
        render_list_screen();
    } else {
        s_list_cache_was_info = true;
        /* Перехід список → інфо: список 6 рядків, інфо 4 — без очищення «хвости» знизу */
        if (menu_screen_type(s_prev_menu_screen) == MENU_SCREEN_LIST)
            lcd_clear_rows();
        render_info_screen(cur);
    }
}

void screens_init(void)
{
    lcd_init();
    touch_init();
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

        if (cur != s_prev_menu_screen)
            need_render = true;

        /* Тачскрін: дотик = вибір рядка (список) або Back (інфо-екран) */
        {
            touch_point_t tp;
            uint32_t now = HAL_GetTick();
            if ((now - touch_last_handled) >= TOUCH_COOLDOWN_MS && touch_read(&tp) && tp.pressed) {
                touch_last_handled = now;
                if (st == MENU_SCREEN_LIST) {
                    unsigned int n = menu_get_count();
                    if (n > 0u) {
                        unsigned int sel = menu_get_selected();
                        unsigned int first = 0u;
                        if (n > 6u) {
                            first = (sel >= 5u) ? (sel - 5u) : 0u;
                            if (first + 6u > n) first = n - 6u;
                        }
                        int row = (int)(tp.y - LCD_OFFSET_Y) / (int)LCD_ROW_HEIGHT;
                        if (row >= 0 && row < 6) {
                            unsigned int idx = first + (unsigned int)row;
                            if (idx < n) {
                                menu_set_selected(idx);
                                menu_enter();
                                need_render = true;
                            }
                        }
                    }
                } else if (menu_can_back()) {
                    if (cur == SCREEN_JOG || cur == SCREEN_FEED_AUTO)
                        axis_feedback_save_last_displayed();
                    menu_back();
                    need_render = true;
                }
            }
        }

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
                /* Up (CCW) або Enter = вихід назад. CW лишаємо для Jog (рух). */
                if (act == ENCODER_MENU_ACTION_CCW || act == ENCODER_MENU_ACTION_ENTER) {
                    if (cur == SCREEN_JOG || cur == SCREEN_FEED_AUTO)
                        axis_feedback_save_last_displayed();
                    menu_back();
                    need_render = true;
                }
            } else if ((cur == SCREEN_AXIS_X || cur == SCREEN_AXIS_Z) && menu_can_back()) {
                axis_id_t axis = (cur == SCREEN_AXIS_X) ? AXIS_X : AXIS_Z;

                if (act == ENCODER_MENU_ACTION_ENTER) {
                    axis_edit_field = (uint8_t)((axis_edit_field + 1u) % 3u);
                    need_render = true;
                } else if (axis_edit_field == 0u && act == ENCODER_MENU_ACTION_CCW) {
                    menu_back();
                    need_render = true;
                } else {
                    /* UP (CCW) = +1, DOWN (CW) = -1 — логічно для користувача */
                    int32_t dir = (act == ENCODER_MENU_ACTION_CCW) ? +1 : -1;
                    const axis_cfg_t *ac = system_axis_cfg(axis);
                    float v;
                    if (axis_edit_field == 0u) {
                        v = ac->max_feed + (float)(dir * 100);
                        if (v < 100.0f) v = 100.0f;
                        if (v > 20000.0f) v = 20000.0f;
                        system_axis_set_max_feed(axis, v);
                    } else if (axis_edit_field == 1u) {
                        v = ac->min_mm + (float)dir;
                        if (v < -2000.0f) v = -2000.0f;
                        if (v > 2000.0f) v = 2000.0f;
                        system_axis_set_min_mm(axis, v);
                    } else {
                        v = ac->max_mm + (float)dir;
                        if (v < -2000.0f) v = -2000.0f;
                        if (v > 2000.0f) v = 2000.0f;
                        system_axis_set_max_mm(axis, v);
                    }
                    need_render = true;
                }
            } else if (cur == SCREEN_MECHANICS && menu_can_back()) {
                axis_id_t axis = (mech_edit_axis == 0u) ? AXIS_X : AXIS_Z;

                if (act == ENCODER_MENU_ACTION_ENTER) {
                    mech_edit_field = (uint8_t)((mech_edit_field + 1u) & 3u);
                    need_render = true;
                } else if (mech_edit_field == 0u && act == ENCODER_MENU_ACTION_CCW) {
                    /* На полі axis: CCW вихід назад. */
                    menu_back();
                    need_render = true;
                } else {
                    /* UP (CCW) = +1, DOWN (CW) = -1 */
                    int32_t dir = (act == ENCODER_MENU_ACTION_CCW) ? +1 : -1;
                    if (mech_edit_field == 0u) {
                        /* Axis toggle */
                        mech_edit_axis ^= 1u;
                        jog_update_steps_per_mm_from_cfg();
                        need_render = true;
                    } else if (mech_edit_field == 1u) {
                        /* Pitch: мм, точність 0.01 */
                        int32_t v = (int32_t)system_mech_get_pitch_x100(axis);
                        v += dir * 1; /* 0.01 мм */
                        if (v < 100) v = 100;
                        if (v > 2000) v = 2000; /* 20.00 мм */
                        system_mech_set_pitch_x100(axis, (uint16_t)v);
                        jog_update_steps_per_mm_from_cfg();
                        need_render = true;
                    } else if (mech_edit_field == 2u) {
                        /* Microstep */
                        int32_t v = (int32_t)system_mech_get_microstep(axis);
                        v += dir * 1;
                        if (v < 1) v = 1;
                        if (v > 256) v = 256;
                        system_mech_set_microstep(axis, (uint16_t)v);
                        jog_update_steps_per_mm_from_cfg();
                        need_render = true;
                    } else if (mech_edit_field == 3u) {
                        /* Reducer ratio: integer (1..20), step = 1 */
                        int32_t ri = (int32_t)(system_mech_get_reducer_ratio_x1000(axis) / 1000u);
                        ri += dir;
                        if (ri < 1) ri = 1;
                        if (ri > 20) ri = 20;
                        system_mech_set_reducer_ratio_x1000(axis, (uint32_t)(ri * 1000u));
                        jog_update_steps_per_mm_from_cfg();
                        need_render = true;
                    }
                }
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
                    /* Up на першому пункті підменю = вихід назад */
                    if (menu_can_back() && menu_get_selected() == 0u) {
                        menu_back();
                    } else {
                        menu_select_prev();
                    }
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
        /* Після menu_back() cur ще старий — інакше наступний кадр знову need_render (подвійне оновлення). */
        s_prev_menu_screen = menu_current_screen();
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
