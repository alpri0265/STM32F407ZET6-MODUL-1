/**
 * @file tft_config.h
 * @brief Конфігурація пінів для TFT 3.2" ILI9341 + XPT2046 touch (SPI)
 *
 * Software SPI. PE8 зайнятий (SPINDLE_ON).
 *
 * ═══════════════════════════════════════════════════════════════════
 * ПІДКЛЮЧЕННЯ (спільні лінії — один дріт до обох контактів модуля):
 * ═══════════════════════════════════════════════════════════════════
 *
 * ДИСПЛЕЙ (ILI9341)          ТАЧ (XPT2046)           STM32
 * ─────────────────          ─────────────           ─────
 * VCC                         —                      3.3V або 5V
 * GND                         —                      GND
 * CS                          —                      PE10
 * RESET                       —                      PE12
 * DC                          —                      PE11
 * SDI(MOSI)  ◄───┬──────────── T_DIN  ◄───────────── PE9   (спільно!)
 * SCK        ◄───┴──────────── T_CLK  ◄───────────── PE7   (спільно!)
 * LED                         —                      PE13
 * SDO(MISO)  не підключати    —                      —
 * —                           T_CS   ◄───────────── PE14
 * —                           T_DO   ─────────────► PE15  (MISO, тільки тач)
 * —                           T_IRQ  (не підключати PE6 — на платі PE6 = ENC_STEP_S2!)
 *
 * Схема спільних пінів: PE7 і PE9 йдуть паралельно до SCK+T_CLK та SDI+T_DIN.
 * Різні CS (PE10 для дисплея, PE14 для тача) вибирають пристрій.
 * ═══════════════════════════════════════════════════════════════════
 */
#ifndef TFT_CONFIG_H
#define TFT_CONFIG_H

#include "stm32f4xx_hal.h"

#define TFT_SCK_PORT   GPIOE
#define TFT_SCK_PIN    GPIO_PIN_7
#define TFT_MOSI_PORT  GPIOE
#define TFT_MOSI_PIN   GPIO_PIN_9
#define TFT_CS_PORT    GPIOE
#define TFT_CS_PIN     GPIO_PIN_10
#define TFT_DC_PORT    GPIOE
#define TFT_DC_PIN     GPIO_PIN_11
#define TFT_RST_PORT   GPIOE
#define TFT_RST_PIN    GPIO_PIN_12
#define TFT_BL_PORT    GPIOE
#define TFT_BL_PIN     GPIO_PIN_13

#define TFT_WIDTH  320   /* ландшафт після MADCTL 0x68 */
#define TFT_HEIGHT 240

/* XPT2046 touch (SPI, спільні SCK/MOSI з дисплеєм) */
#define TOUCH_CS_PORT   GPIOE
#define TOUCH_CS_PIN    GPIO_PIN_14
#define TOUCH_MISO_PORT GPIOE
#define TOUCH_MISO_PIN  GPIO_PIN_15
/* T_IRQ не використовуємо — PE6 на MODUL-1 зайнятий енкодером (див. main.h). */

/* Калібрування XPT2046 (підлаштуйте під свій модуль, якщо точки "їдуть") */
#define TOUCH_X_MIN  300
#define TOUCH_X_MAX  3900
#define TOUCH_Y_MIN  400
#define TOUCH_Y_MAX  3900
/* Якщо X/Y замінені або інвертовані — розкоментуйте: */
/* #define TOUCH_SWAP_XY  1 */
/* #define TOUCH_INVERT_X 1 */
/* #define TOUCH_INVERT_Y 1 */

#endif /* TFT_CONFIG_H */
