#ifndef BOARD_H
#define BOARD_H
#include "stm32f4xx_hal.h"

/* Крокові виходи: узгоджено з main.h (PA8/PA9/PA10, PB6/PB7/PB8) */
#define X_STEP_PORT GPIOA
#define X_STEP_PIN  GPIO_PIN_8
#define X_DIR_PORT  GPIOA
#define X_DIR_PIN   GPIO_PIN_9

#define Z_STEP_PORT GPIOB
#define Z_STEP_PIN  GPIO_PIN_6
#define Z_DIR_PORT  GPIOB
#define Z_DIR_PIN   GPIO_PIN_7

#define ESTOP_PORT  GPIOD
/* ADC channels mapping: logical 0=PA0, 1=PA1, 2=PA2, 3=PA3, 4=PA4, 5=PA5 */
#define ADC_CH_FEED_OVERRIDE   4   /* PA4 = FEED_ADC (корекція подачі) */
#define ADC_CH_TOOL_ANGLE      5   /* PA5, ADC1_IN5 — абсолютний енкодер (кут інструменту), 0..360 deg */
#define ADC_CH_TEMPERATURE     3
#define ADC_CH_POWER_MONITOR   4

#define ESTOP_PIN   GPIO_PIN_0

/* LCD 2004 I2C (20x4): PB10 = SCL, PB11 = SDA, I2C2 */
#define LCD_I2C         I2C2
#define LCD_I2C_SCL_PIN GPIO_PIN_10
#define LCD_I2C_SCL_PORT GPIOB
#define LCD_I2C_SDA_PIN GPIO_PIN_11
#define LCD_I2C_SDA_PORT GPIOB

#endif
