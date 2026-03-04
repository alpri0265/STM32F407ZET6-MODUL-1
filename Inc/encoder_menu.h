#ifndef ENCODER_MENU_H
#define ENCODER_MENU_H

void encoder_menu_init(void);
void encoder_menu_process(void);

typedef enum {
    ENCODER_MENU_ACTION_NONE = 0,
    ENCODER_MENU_ACTION_CW,
    ENCODER_MENU_ACTION_CCW,
    ENCODER_MENU_ACTION_ENTER
} encoder_menu_action_t;

encoder_menu_action_t encoder_menu_get_action(void);
void encoder_menu_clear_action(void);

#endif
