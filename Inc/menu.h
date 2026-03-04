#ifndef MENU_H
#define MENU_H

#define MENU_ITEM_MAX  5
#define MENU_ITEM_LEN  16

void menu_init(void);
void menu_select_next(void);
void menu_select_prev(void);
void menu_enter(void);

unsigned int menu_get_count(void);
unsigned int menu_get_selected(void);
const char *menu_get_item_text(unsigned int index);

#endif
