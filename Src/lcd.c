#include "lcd.h"
#include "main.h"                 // важливо: тут hi2c2
#include "stm32f4xx_hal.h"

extern I2C_HandleTypeDef hi2c2;   // у вас I2C2

#define LCD_ADDR (0x27 << 1)      // якщо не працює → 0x3F << 1

static void lcd_send(uint8_t data, uint8_t rs)
{
    uint8_t high = data & 0xF0;
    uint8_t low  = (data << 4) & 0xF0;

    uint8_t buf[4];

    buf[0] = high | rs | 0x04 | 0x08;
    buf[1] = high | rs | 0x08;
    buf[2] = low  | rs | 0x04 | 0x08;
    buf[3] = low  | rs | 0x08;

    HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDR, buf, 4, 100);
}

/* Запис символу градуса в CGRAM слот 0 (код 0x00). */
static void lcd_load_degree_char(void)
{
    lcd_send(0x40, 0);  /* CGRAM address 0 */
    /* Градус: коло 5x8 */
    lcd_send(0x0E, 1);
    lcd_send(0x11, 1);
    lcd_send(0x11, 1);
    lcd_send(0x0E, 1);
    lcd_send(0x00, 1);
    lcd_send(0x00, 1);
    lcd_send(0x00, 1);
    lcd_send(0x00, 1);
    lcd_send(0x80, 0);  /* повернути адресу в DDRAM (рядок 0), щоб не губились перші символи */
}

void lcd_init(void)
{
    HAL_Delay(50);
    lcd_send(0x33, 0);
    lcd_send(0x32, 0);
    lcd_send(0x28, 0);
    lcd_send(0x0C, 0);
    lcd_send(0x06, 0);
    lcd_send(0x01, 0);
    HAL_Delay(5);
}

void lcd_clear(void)
{
    lcd_send(0x01, 0);
    HAL_Delay(5);
}

void lcd_print(const char *str)
{
    while (*str)
        lcd_send((uint8_t)*str++, 1);
}

static const uint8_t row_addr[] = { 0x00, 0x40, 0x14, 0x54 };

#define LCD_COLS 20

void lcd_print_line(uint8_t row, const char *str)
{
    unsigned int n = 0;
    if (row > 3) return;
    lcd_send(0x80 | row_addr[row], 0);
    while (*str && n < LCD_COLS) {
        lcd_send((uint8_t)*str++, 1);
        n++;
    }
}

#define DEG_PLACEHOLDER 0xFF

void lcd_print_line_deg(uint8_t row, const char *str)
{
    static uint8_t degree_loaded;
    if (row > 3) return;
    if (!degree_loaded) {
        lcd_load_degree_char();
        degree_loaded = 1;
    }
    lcd_send(0x80 | row_addr[row], 0);
    while (*str) {
        uint8_t c = (uint8_t)*str++;
        lcd_send((c == DEG_PLACEHOLDER) ? 0u : c, 1);
    }
}

