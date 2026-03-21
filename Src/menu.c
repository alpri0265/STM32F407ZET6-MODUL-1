#include "menu.h"
#include "system_config.h"
#include <string.h>


static const char *main_items[]     = { "Jog", "Feed", "Settings", "Diagnostics", "Tool angle", "Info", "" };
static const char *feed_items[]     = { "Manual feed", "Automatic feed", "Z passes", "X passes", "< Back", "" };
static const char *settings_items[] = { "Axis X", "Axis Z", "Mechanics", "Save & exit", "< Back", "" };
static const char *diag_items[]     = { "I2C / LCD", "Encoders", "Limits", "SL test", "ADC / Fault", "Tool angle calib", "< Back", "" };

static menu_screen_id_t main_children[]      = { SCREEN_JOG, SCREEN_FEED, SCREEN_SETTINGS, SCREEN_DIAG, SCREEN_TOOL_ANGLE, SCREEN_INFO };
static menu_screen_id_t feed_children[]      = { SCREEN_FEED_MANUAL, SCREEN_FEED_AUTO, SCREEN_Z_PASSES, SCREEN_X_PASSES, SCREEN_ACTION_BACK };
static menu_screen_id_t settings_children[]  = { SCREEN_AXIS_X, SCREEN_AXIS_Z, SCREEN_MECHANICS, SCREEN_ACTION_SAVE_EXIT, SCREEN_ACTION_BACK };
static menu_screen_id_t diag_children[]       = { SCREEN_I2C_LCD, SCREEN_ENCODERS, SCREEN_LIMITS, SCREEN_SL_TEST, SCREEN_ADC_FAULT, SCREEN_TOOL_ANGLE_CALIB, SCREEN_ACTION_BACK };

static menu_screen_id_t stack[MENU_STACK_MAX];
static unsigned int stack_top;
static unsigned int selected;

static unsigned int list_count(const char **items)
{
    unsigned int n = 0;
    while (items[n][0] != '\0' && n < MENU_ITEM_MAX) n++;
    return n;
}

static menu_screen_id_t list_child(menu_screen_id_t screen, unsigned int index)
{
    switch (screen) {
        case SCREEN_MAIN:
            if (index < 6u) return main_children[index];
            break;
        case SCREEN_FEED:
            if (index < 5u) return feed_children[index];
            break;
        case SCREEN_SETTINGS:
            if (index < 5u) return settings_children[index];
            break;
        case SCREEN_DIAG:
            if (index < 7u) return diag_children[index];
            break;
        default:
            break;
    }
    return SCREEN_NONE;
}

void menu_init(void)
{
    stack[0] = SCREEN_MAIN;
    stack_top = 0;
    selected = 0;
}

void menu_select_next(void)
{
    menu_screen_id_t cur = stack[stack_top];
    unsigned int n = 0;
    if (menu_screen_type(cur) == MENU_SCREEN_LIST) {
        if (cur == SCREEN_MAIN) n = list_count(main_items);
        else if (cur == SCREEN_FEED) n = list_count(feed_items);
        else if (cur == SCREEN_SETTINGS) n = list_count(settings_items);
        else if (cur == SCREEN_DIAG) n = list_count(diag_items);
        if (n > 0u) {
            selected++;
            if (selected >= n) selected = 0;
        }
    }
}

void menu_select_prev(void)
{
    menu_screen_id_t cur = stack[stack_top];
    unsigned int n = 0;
    if (menu_screen_type(cur) == MENU_SCREEN_LIST) {
        if (cur == SCREEN_MAIN) n = list_count(main_items);
        else if (cur == SCREEN_FEED) n = list_count(feed_items);
        else if (cur == SCREEN_SETTINGS) n = list_count(settings_items);
        else if (cur == SCREEN_DIAG) n = list_count(diag_items);
        if (n > 0u) {
            if (selected == 0) selected = n - 1;
            else selected--;
        }
    }
}

void menu_set_selected(unsigned int index)
{
    menu_screen_id_t cur = stack[stack_top];
    unsigned int n = 0;
    if (menu_screen_type(cur) == MENU_SCREEN_LIST) {
        if (cur == SCREEN_MAIN) n = list_count(main_items);
        else if (cur == SCREEN_FEED) n = list_count(feed_items);
        else if (cur == SCREEN_SETTINGS) n = list_count(settings_items);
        else if (cur == SCREEN_DIAG) n = list_count(diag_items);
        if (index < n) selected = index;
    }
}

void menu_enter(void)
{
    menu_screen_id_t cur = stack[stack_top];
    if (menu_screen_type(cur) != MENU_SCREEN_LIST) return;

    menu_screen_id_t child = list_child(cur, selected);
    if (child == SCREEN_NONE) return;
    if (child == SCREEN_ACTION_SAVE_EXIT) {
        system_config_save();
        stack_top = 0;
        stack[0] = SCREEN_MAIN;
        selected = 0;
        return;
    }
    if (child == SCREEN_ACTION_BACK) {
        menu_back();
        return;
    }
    if (stack_top + 1u < MENU_STACK_MAX) {
        stack_top++;
        stack[stack_top] = child;
        selected = 0;
    }
}

void menu_back(void)
{
    if (stack_top > 0u) {
        stack_top--;
        selected = 0;
    }
}

menu_screen_id_t menu_current_screen(void)
{
    return stack[stack_top];
}

menu_screen_type_t menu_screen_type(menu_screen_id_t id)
{
    switch (id) {
        case SCREEN_MAIN:
        case SCREEN_FEED:
        case SCREEN_SETTINGS:
        case SCREEN_DIAG:
            return MENU_SCREEN_LIST;
        case SCREEN_TOOL_ANGLE:
        case SCREEN_TOOL_ANGLE_CALIB:
        case SCREEN_Z_PASSES:
        case SCREEN_X_PASSES:
        case SCREEN_MECHANICS:
        case SCREEN_AXIS_X:
        case SCREEN_AXIS_Z:
        case SCREEN_INFO:
        default:
            return MENU_SCREEN_INFO;
    }
}

unsigned int menu_get_count(void)
{
    menu_screen_id_t cur = stack[stack_top];
    if (cur == SCREEN_MAIN) return list_count(main_items);
    if (cur == SCREEN_FEED) return list_count(feed_items);
    if (cur == SCREEN_SETTINGS) return list_count(settings_items);
    if (cur == SCREEN_DIAG) return list_count(diag_items);
    return 0;
}

unsigned int menu_get_selected(void)
{
    return selected;
}

const char *menu_get_item_text(unsigned int index)
{
    menu_screen_id_t cur = stack[stack_top];
    if (cur == SCREEN_MAIN && index < list_count(main_items)) return main_items[index];
    if (cur == SCREEN_FEED && index < list_count(feed_items)) return feed_items[index];
    if (cur == SCREEN_SETTINGS && index < list_count(settings_items)) return settings_items[index];
    if (cur == SCREEN_DIAG && index < list_count(diag_items)) return diag_items[index];
    return "";
}

int menu_can_back(void)
{
    return stack_top > 0 ? 1 : 0;
}
