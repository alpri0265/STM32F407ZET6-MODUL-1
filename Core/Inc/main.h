/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define FEED_MODE_Pin GPIO_PIN_2
#define FEED_MODE_GPIO_Port GPIOE
#define ENC_AXIS_S1_Pin GPIO_PIN_3
#define ENC_AXIS_S1_GPIO_Port GPIOE
#define ENC_AXIS_S2_Pin GPIO_PIN_4
#define ENC_AXIS_S2_GPIO_Port GPIOE
#define ENC_STEP_S1_Pin GPIO_PIN_5
#define ENC_STEP_S1_GPIO_Port GPIOE
#define ENC_STEP_S2_Pin GPIO_PIN_6
#define ENC_STEP_S2_GPIO_Port GPIOE
#define SCALE_0_Pin GPIO_PIN_0
#define SCALE_0_GPIO_Port GPIOC
#define SCA_Pin GPIO_PIN_1
#define SCA_GPIO_Port GPIOC
#define JOY_UP_Pin GPIO_PIN_2
#define JOY_UP_GPIO_Port GPIOC
#define JOY_DOWN_Pin GPIO_PIN_3
#define JOY_DOWN_GPIO_Port GPIOC
#define ENC_X_A_Pin GPIO_PIN_0
#define ENC_X_A_GPIO_Port GPIOA
#define ENC_X_B_Pin GPIO_PIN_1
#define ENC_X_B_GPIO_Port GPIOA
#define TEMP_ADC_Pin GPIO_PIN_2
#define TEMP_ADC_GPIO_Port GPIOA
#define SPINDLE_PWM_Pin GPIO_PIN_3
#define SPINDLE_PWM_GPIO_Port GPIOA
#define FEED_ADC_Pin GPIO_PIN_4
#define FEED_ADC_GPIO_Port GPIOA
#define TOOL_ANGLE_ADC_Pin GPIO_PIN_5
#define TOOL_ANGLE_ADC_GPIO_Port GPIOA
#define ENC_Z_A_Pin GPIO_PIN_6
#define ENC_Z_A_GPIO_Port GPIOA
#define ENC_Z_B_Pin GPIO_PIN_7
#define ENC_Z_B_GPIO_Port GPIOA
#define JOY_LEFT_Pin GPIO_PIN_4
#define JOY_LEFT_GPIO_Port GPIOC
#define JOY_RIGHT_Pin GPIO_PIN_5
#define JOY_RIGHT_GPIO_Port GPIOC
#define SPINDLE_ON_Pin GPIO_PIN_8
#define SPINDLE_ON_GPIO_Port GPIOE
#define I2C2_SCL_Pin GPIO_PIN_10
#define I2C2_SCL_GPIO_Port GPIOB
#define I2C2_SDA_Pin GPIO_PIN_11
#define I2C2_SDA_GPIO_Port GPIOB
#define MPG_A_Pin GPIO_PIN_12
#define MPG_A_GPIO_Port GPIOB
#define MPG_B_Pin GPIO_PIN_13
#define MPG_B_GPIO_Port GPIOB
#define MPG_BTN_Pin GPIO_PIN_14
#define MPG_BTN_GPIO_Port GPIOB
#define MENU_UP_Pin GPIO_PIN_8
#define MENU_UP_GPIO_Port GPIOD
#define MENU_DOWN_Pin GPIO_PIN_9
#define MENU_DOWN_GPIO_Port GPIOD
#define MENU_ENTER_Pin GPIO_PIN_10
#define MENU_ENTER_GPIO_Port GPIOD
#define SL_X_NEG_BIT_Pin GPIO_PIN_7
#define SL_X_NEG_BIT_GPIO_Port GPIOC
#define SL_X_POS_BIT_Pin GPIO_PIN_8
#define SL_X_POS_BIT_GPIO_Port GPIOC
#define SL_Z_NEG_BIT_Pin GPIO_PIN_9
#define SL_Z_NEG_BIT_GPIO_Port GPIOC
#define X_STEP_Pin GPIO_PIN_8
#define X_STEP_GPIO_Port GPIOA
#define X_DIR_Pin GPIO_PIN_9
#define X_DIR_GPIO_Port GPIOA
#define X_EN_Pin GPIO_PIN_10
#define X_EN_GPIO_Port GPIOA
#define SL_Z_POS_BIT_Pin GPIO_PIN_10
#define SL_Z_POS_BIT_GPIO_Port GPIOC
#define MODE_AXIS_Pin GPIO_PIN_11
#define MODE_AXIS_GPIO_Port GPIOC
#define SL_X_NEG_LED_Pin GPIO_PIN_0
#define SL_X_NEG_LED_GPIO_Port GPIOD
#define SL_X_POS_LED_Pin GPIO_PIN_1
#define SL_X_POS_LED_GPIO_Port GPIOD
#define SL_Z_NEG_LED_Pin GPIO_PIN_2
#define SL_Z_NEG_LED_GPIO_Port GPIOD
#define SL_Z_POS_LED_Pin GPIO_PIN_3
#define SL_Z_POS_LED_GPIO_Port GPIOD
#define LIM_X_NEG_Pin GPIO_PIN_4
#define LIM_X_NEG_GPIO_Port GPIOD
#define LIM_X_POS_Pin GPIO_PIN_5
#define LIM_X_POS_GPIO_Port GPIOD
#define LIM_Z_NEG_Pin GPIO_PIN_6
#define LIM_Z_NEG_GPIO_Port GPIOD
#define LIM_Z_POS_Pin GPIO_PIN_7
#define LIM_Z_POS_GPIO_Port GPIOD
#define z_STEP_Pin GPIO_PIN_6
#define z_STEP_GPIO_Port GPIOB
#define Z_DIR_Pin GPIO_PIN_7
#define Z_DIR_GPIO_Port GPIOB
#define Z_EN_Pin GPIO_PIN_8
#define Z_EN_GPIO_Port GPIOB
#define E_STOP_Pin GPIO_PIN_0
#define E_STOP_GPIO_Port GPIOE
#define E_STOP_EXTI_IRQn EXTI0_IRQn
#define FAULT_IN_Pin GPIO_PIN_1
#define FAULT_IN_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
