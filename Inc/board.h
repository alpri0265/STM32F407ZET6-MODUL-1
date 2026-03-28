#ifndef BOARD_H
#define BOARD_H
#include "stm32f4xx_hal.h"

/*
 * Пульт ZSY1474: перемикач OFF / X / Y / Z / 4 (COM на GND, лінії з підтяжкою на MCU).
 * OFF = автоматичний режим подачі; X/Z = ручний MPG по відповідній осі.
 * PE2 (FEED_MODE) у цьому режимі не читається — залиште непідключеним або на фіксованому рівні.
 * Y та «4» у прошивці лише блокують хибне «OFF»; рух по них не реалізований (тільки X/Z).
 * Якщо фізично не підводите дроти Y і 4 — не використовуйте ці позиції перемикача (інакше
 * помилково буде AUTO). Або задайте MANUAL_FEED_PENDANT_AXIS_NEED_Y4 0 і не ставте перемикач на Y/4.
 */
#ifndef MANUAL_FEED_USE_PENDANT_AXIS_SWITCH
#define MANUAL_FEED_USE_PENDANT_AXIS_SWITCH 0
#endif
#if MANUAL_FEED_USE_PENDANT_AXIS_SWITCH
#ifndef MANUAL_FEED_PENDANT_AXIS_NEED_Y4
#define MANUAL_FEED_PENDANT_AXIS_NEED_Y4 1
#endif
#define PENDAXIS_Y_GPIO_Port  GPIOA
#define PENDAXIS_Y_Pin        GPIO_PIN_2
#define PENDAXIS_4_GPIO_Port  GPIOA
#define PENDAXIS_4_Pin        GPIO_PIN_3
#endif

/*
 * Множник пульта ZSY1474: X1 / X10 / X100 (COM на GND, one-hot, підтяжка на MCU).
 * Відповідність у jog: X1 → 0.001 мм, X10 → 0.01 мм, X100 → 0.1 мм.
 * Grey → PE5, Black/Grey → PE6, Orange → PE7.
 * За замовчуванням увімкнено разом з MANUAL_FEED_USE_PENDANT_AXIS_SWITCH; можна
 * перевизначити MANUAL_FEED_USE_PENDANT_STEP_SWITCH окремо (0 = стара 2-дротова схема PE5/PE6).
 */
#ifndef MANUAL_FEED_USE_PENDANT_STEP_SWITCH
#define MANUAL_FEED_USE_PENDANT_STEP_SWITCH MANUAL_FEED_USE_PENDANT_AXIS_SWITCH
#endif
#if MANUAL_FEED_USE_PENDANT_STEP_SWITCH
#define PENDSTEP_X1_GPIO_Port   GPIOE
#define PENDSTEP_X1_Pin         GPIO_PIN_5
#define PENDSTEP_X10_GPIO_Port  GPIOE
#define PENDSTEP_X10_Pin        GPIO_PIN_6
#define PENDSTEP_X100_GPIO_Port GPIOE
#define PENDSTEP_X100_Pin       GPIO_PIN_7
#endif

/* Крокові виходи: узгоджено з main.h (PA8/PA9/PA10, PB6/PB7/PB8) */
#define X_STEP_PORT GPIOA
#define X_STEP_PIN  GPIO_PIN_8
#define X_DIR_PORT  GPIOA
#define X_DIR_PIN   GPIO_PIN_9

#define Z_STEP_PORT GPIOB
#define Z_STEP_PIN  GPIO_PIN_6
#define Z_DIR_PORT  GPIOB
#define Z_DIR_PIN   GPIO_PIN_7

/* Аварійний стоп: пін як у main.h — E_STOP = PE0.
 * Пульт ZSY1474 (C / CN): контакт NC. Типово один вивід NC на GND, інший на PE0,
 * у MCU GPIO_PULLUP + EXTI RISING. У нормі ланцюг замкнений → PE0=LOW; стоп → розрив → HIGH.
 * Панельний стоп послідовно в той самий NC-ланцюг. Якщо у вас NO або інша полярність — ESTOP_ACTIVE_HIGH=0. */
#ifndef ESTOP_ACTIVE_HIGH
#define ESTOP_ACTIVE_HIGH 1
#endif

/* ADC channels mapping: logical 0=PA0, 1=PA1, 2=PA2, 3=PA3, 4=PA4, 5=PA5 */
#define ADC_CH_FEED_OVERRIDE   4   /* PA4 = FEED_ADC (корекція подачі) */
#define ADC_CH_TOOL_ANGLE      5   /* PA5, ADC1_IN5 — абсолютний енкодер (кут інструменту), 0..360 deg */
#define ADC_CH_POWER_MONITOR   4

/* LCD 2004 I2C (20x4): PB10 = SCL, PB11 = SDA, I2C2 */
#define LCD_I2C         I2C2
#define LCD_I2C_SCL_PIN GPIO_PIN_10
#define LCD_I2C_SCL_PORT GPIOB
#define LCD_I2C_SDA_PIN GPIO_PIN_11
#define LCD_I2C_SDA_PORT GPIOB

#endif
