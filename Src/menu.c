#include "menu.h"
#include <string.h>

static const char *items[] = {
    "I2C OK",
    "Encoder OK",
    "Settings",
    "Diagnostics",
    ""
};

static unsigned int count;
static unsigned int selected;

void menu_init(void)
{
    count = 0;
    while (items[count][0] != '\0' && count < MENU_ITEM_MAX)
        count++;
    if (count == 0) count = 1;
    selected = 0;
}

void menu_select_next(void)
{
    selected++;
    if (selected >= count) selected = 0;
}

void menu_select_prev(void)
{
    if (selected == 0)
        selected = count - 1;
    else
        selected--;
}

void menu_enter(void)
{
    (void)selected;
}

unsigned int menu_get_count(void)
{
    return count;
}

unsigned int menu_get_selected(void)
{
    return selected;
}

const char *menu_get_item_text(unsigned int index)
{
    if (index >= count) return "";
    return items[index];
}
