#ifndef LCD_H
#define LCD_H
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"


void lcd_init(void);
void lcd_clear(void);
void lcd_print(const char *str);
void lcd_print_line(uint8_t row, const char *str);

#endif
