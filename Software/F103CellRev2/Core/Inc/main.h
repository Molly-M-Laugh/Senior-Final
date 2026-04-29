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
#include "stm32f1xx_hal.h"

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
#define OnboardLED_Pin GPIO_PIN_13
#define OnboardLED_GPIO_Port GPIOC
#define TEMP2_ALERT_Pin GPIO_PIN_14
#define TEMP2_ALERT_GPIO_Port GPIOC
#define TEMP2_ALERT_EXTI_IRQn EXTI15_10_IRQn
#define CAN_FLT_Pin GPIO_PIN_15
#define CAN_FLT_GPIO_Port GPIOC
#define CAN_FLT_EXTI_IRQn EXTI15_10_IRQn
#define WAKE_Pin GPIO_PIN_0
#define WAKE_GPIO_Port GPIOA
#define FLT_3V3_Pin GPIO_PIN_1
#define FLT_3V3_GPIO_Port GPIOA
#define FLT_3V3_EXTI_IRQn EXTI1_IRQn
#define FLT_1V8_Pin GPIO_PIN_2
#define FLT_1V8_GPIO_Port GPIOA
#define FLT_1V8_EXTI_IRQn EXTI2_IRQn
#define INA3221_CRIT_Pin GPIO_PIN_3
#define INA3221_CRIT_GPIO_Port GPIOA
#define INA3221_CRIT_EXTI_IRQn EXTI3_IRQn
#define SPI_CS_Pin GPIO_PIN_4
#define SPI_CS_GPIO_Port GPIOA
#define MCU_SCLK_Pin GPIO_PIN_5
#define MCU_SCLK_GPIO_Port GPIOA
#define MCU_MISO_Pin GPIO_PIN_6
#define MCU_MISO_GPIO_Port GPIOA
#define MCU_MOSI_Pin GPIO_PIN_7
#define MCU_MOSI_GPIO_Port GPIOA
#define FLT_5V0_Pin GPIO_PIN_0
#define FLT_5V0_GPIO_Port GPIOB
#define FLT_5V0_EXTI_IRQn EXTI0_IRQn
#define PG3V3_Pin GPIO_PIN_1
#define PG3V3_GPIO_Port GPIOB
#define EN5V0_Pin GPIO_PIN_10
#define EN5V0_GPIO_Port GPIOB
#define OSC_EN_Pin GPIO_PIN_11
#define OSC_EN_GPIO_Port GPIOB
#define TEMP0_ALERT_Pin GPIO_PIN_12
#define TEMP0_ALERT_GPIO_Port GPIOB
#define TEMP0_ALERT_EXTI_IRQn EXTI15_10_IRQn
#define TEMP1_ALERT_Pin GPIO_PIN_13
#define TEMP1_ALERT_GPIO_Port GPIOB
#define TEMP1_ALERT_EXTI_IRQn EXTI15_10_IRQn
#define ON1V8_Pin GPIO_PIN_14
#define ON1V8_GPIO_Port GPIOB
#define EN3V3_Pin GPIO_PIN_15
#define EN3V3_GPIO_Port GPIOB
#define FreeIO_Pin GPIO_PIN_8
#define FreeIO_GPIO_Port GPIOA
#define INA3221_VALID_Pin GPIO_PIN_9
#define INA3221_VALID_GPIO_Port GPIOA
#define INA3221_VALID_EXTI_IRQn EXTI9_5_IRQn
#define INA3221_WARN_Pin GPIO_PIN_10
#define INA3221_WARN_GPIO_Port GPIOA
#define INA3221_WARN_EXTI_IRQn EXTI15_10_IRQn
#define USB__Pin GPIO_PIN_11
#define USB__GPIO_Port GPIOA
#define USB_A12_Pin GPIO_PIN_12
#define USB_A12_GPIO_Port GPIOA
#define WP_Pin GPIO_PIN_15
#define WP_GPIO_Port GPIOA
#define SingleWireOutput_Pin GPIO_PIN_3
#define SingleWireOutput_GPIO_Port GPIOB
#define PG5V0_Pin GPIO_PIN_4
#define PG5V0_GPIO_Port GPIOB
#define PG5V0_EXTI_IRQn EXTI4_IRQn
#define CAN_ALRT0_Pin GPIO_PIN_5
#define CAN_ALRT0_GPIO_Port GPIOB
#define CAN_ALRT0_EXTI_IRQn EXTI9_5_IRQn
#define CAN_ALRT1_Pin GPIO_PIN_6
#define CAN_ALRT1_GPIO_Port GPIOB
#define CAN_ALRT1_EXTI_IRQn EXTI9_5_IRQn
#define CAN_ALRT2_Pin GPIO_PIN_7
#define CAN_ALRT2_GPIO_Port GPIOB
#define CAN_ALRT2_EXTI_IRQn EXTI9_5_IRQn
#define SCL_Pin GPIO_PIN_8
#define SCL_GPIO_Port GPIOB
#define SDA_Pin GPIO_PIN_9
#define SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
