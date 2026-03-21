#ifndef LCD_H
#define LCD_H
#include "stm32f4xx_hal.h"


void lcd_init(void);
void lcd_clear(void);
void lcd_print(const char *str);
void lcd_print_line(uint8_t row, const char *str);
/* Друкує рядок до row; байт 0xFF у str виводиться як символ градуса (CGRAM 0). */
void lcd_print_line_deg(uint8_t row, const char *str);

#endif
