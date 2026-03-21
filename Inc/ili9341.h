/**
 * @file ili9341.h
 * @brief Драйвер ILI9341 TFT 240x320 (3.2" SPI)
 */
#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>

void ili9341_init(void);
void ili9341_fill_screen(uint16_t color);
void ili9341_fill_rect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color);
void ili9341_draw_pixel(int16_t x, int16_t y, uint16_t color);
void ili9341_draw_char(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg);
void ili9341_draw_string(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg);

/* RGB565 кольори */
#define ILI9341_BLACK   0x0000
#define ILI9341_WHITE   0xFFFF
#define ILI9341_GREEN   0x07E0
#define ILI9341_RED     0xF800
#define ILI9341_BLUE    0x001F
#define ILI9341_YELLOW  0xFFE0

#endif /* ILI9341_H */
