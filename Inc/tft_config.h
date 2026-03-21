/**
 * @file tft_config.h
 * @brief Конфігурація пінів для TFT 3.2" ILI9341 (SPI)
 *
 * Software SPI на вільних пінах (PE7, PE9-PE12).
 * PE8 зайнятий (SPINDLE_ON).
 *
 * Підключення модуля ILI9341:
 *   TFT_SCK  -> PE7
 *   TFT_MOSI -> PE9
 *   TFT_CS   -> PE10
 *   TFT_DC   -> PE11
 *   TFT_RST  -> PE12
 *   TFT_BL   -> PE13  (підсвітка LED, HIGH = вкл.)
 *   VCC 5V (багато модулів потребують 5V!), GND
 *
 * Альтернатива BL: LED модуля → 3.3V або 5V для постійної підсвітки.
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

#endif /* TFT_CONFIG_H */
