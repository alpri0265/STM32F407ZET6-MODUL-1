/**
 * @file lcd.c
 * @brief Обгортка lcd.h для TFT ILI9341 3.2" (SPI)
 * Емулює 6 рядків x 20 символів.
 */
#include "lcd.h"
#include "ili9341.h"
#include "tft_config.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define LCD_ROWS  6
#define LCD_COLS  20
#define FONT_W    12   /* 5x7 масштаб 2x */
#define FONT_H    14
#define ROW_GAP   10   /* відстань між рядками меню (разом з FONT_H = висота кроку) */
#define OFFSET_X  8
#define OFFSET_Y  8

#define ROW_HEIGHT  (FONT_H + ROW_GAP)

/* Координати Y для рядків 0..5 */
static uint16_t row_y[LCD_ROWS];

#define FG_COLOR  ILI9341_WHITE
#define BG_COLOR  ILI9341_BLACK

#define DEG_PLACEHOLDER 0xFF

void lcd_init(void)
{
    ili9341_init();
    ili9341_fill_screen(BG_COLOR);

    for (int i = 0; i < LCD_ROWS; i++) {
        row_y[i] = OFFSET_Y + i * (ROW_HEIGHT);
    }
}

void lcd_clear(void)
{
    ili9341_fill_screen(BG_COLOR);
}

void lcd_clear_rows(void)
{
    for (int i = 0; i < LCD_ROWS; i++) {
        ili9341_fill_rect(0, row_y[i], TFT_WIDTH, ROW_HEIGHT, BG_COLOR);
    }
}

void lcd_print(const char *str)
{
    (void)str;
    /* Не використовується в screens.c; якщо потрібно — можна вивести в рядок 0 */
}

void lcd_print_line(uint8_t row, const char *str)
{
    if (row >= LCD_ROWS) return;

    uint16_t x = OFFSET_X;
    uint16_t y = row_y[row];

    for (unsigned int i = 0; i < LCD_COLS; i++) {
        char c = (str[i] != '\0') ? str[i] : ' ';
        ili9341_draw_char(x, y, c, FG_COLOR, BG_COLOR);
        x += FONT_W;
    }
}

void lcd_print_line_deg(uint8_t row, const char *str)
{
    if (row >= LCD_ROWS) return;

    uint16_t x = OFFSET_X;
    uint16_t y = row_y[row];

    for (unsigned int i = 0; i < LCD_COLS; i++) {
        uint8_t c = (str[i] != '\0') ? (uint8_t)str[i] : (uint8_t)' ';
        ili9341_draw_char(x, y, (char)c, FG_COLOR, BG_COLOR);
        x += FONT_W;
    }
}
