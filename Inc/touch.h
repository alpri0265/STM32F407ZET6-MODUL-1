/**
 * @file touch.h
 * @brief XPT2046 resistive touch (SPI, shared with ILI9341)
 */
#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    bool     pressed;
    uint16_t raw_x; /* сирі ADC (до SWAP/INVERT), для налагодження */
    uint16_t raw_y;
} touch_point_t;

void touch_init(void);
/* Повертає true якщо є дотик, заповнює *p */
bool touch_read(touch_point_t *p);

#endif /* TOUCH_H */
