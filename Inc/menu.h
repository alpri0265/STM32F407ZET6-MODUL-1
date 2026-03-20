#ifndef MENU_H
#define MENU_H

#define MENU_ITEM_MAX  8
#define MENU_ITEM_LEN  20
#define MENU_STACK_MAX 6

typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_JOG,
    SCREEN_FEED,
    SCREEN_FEED_MANUAL,
    SCREEN_FEED_AUTO,
    SCREEN_Z_PASSES,
    SCREEN_X_PASSES,
    SCREEN_SETTINGS,
    SCREEN_AXIS_X,
    SCREEN_AXIS_Z,
    SCREEN_MECHANICS,
    SCREEN_SPINDLE,
    SCREEN_DIAG,
    SCREEN_I2C_LCD,
    SCREEN_ENCODERS,
    SCREEN_LIMITS,
    SCREEN_SL_TEST,
    SCREEN_ADC_FAULT,
    SCREEN_TOOL_ANGLE,
    SCREEN_TOOL_ANGLE_CALIB,
    SCREEN_INFO,
    SCREEN_COUNT,
    SCREEN_NONE = -1,
    SCREEN_ACTION_SAVE_EXIT = -2,
    SCREEN_ACTION_BACK = -3
} menu_screen_id_t;

typedef enum {
    MENU_SCREEN_LIST,
    MENU_SCREEN_INFO
} menu_screen_type_t;

void menu_init(void);
void menu_select_next(void);
void menu_select_prev(void);
void menu_enter(void);
void menu_back(void);

menu_screen_id_t menu_current_screen(void);
menu_screen_type_t menu_screen_type(menu_screen_id_t id);
unsigned int menu_get_count(void);
unsigned int menu_get_selected(void);
const char *menu_get_item_text(unsigned int index);
int menu_can_back(void);

#endif
