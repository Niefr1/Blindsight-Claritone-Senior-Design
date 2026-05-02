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

#if defined ( __ICCARM__ )
#  define CMSE_NS_CALL  __cmse_nonsecure_call
#  define CMSE_NS_ENTRY __cmse_nonsecure_entry
#else
#  define CMSE_NS_CALL  __attribute((cmse_nonsecure_call))
#  define CMSE_NS_ENTRY __attribute((cmse_nonsecure_entry))
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32n6xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* Function pointer declaration in non-secure*/
#if defined ( __ICCARM__ )
typedef void (CMSE_NS_CALL *funcptr)(void);
#else
typedef void CMSE_NS_CALL (*funcptr)(void);
#endif

/* typedef for non-secure callback functions */
typedef funcptr funcptr_NS;

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
#define SCROLL_2_Pin GPIO_PIN_1
#define SCROLL_2_GPIO_Port GPIOC
#define BTN_TONE_Pin GPIO_PIN_4
#define BTN_TONE_GPIO_Port GPIOC
#define TOF_LPN_Pin GPIO_PIN_1
#define TOF_LPN_GPIO_Port GPIOD
#define BTN_SENS_Pin GPIO_PIN_6
#define BTN_SENS_GPIO_Port GPIOC
#define SCROLL_NO_Pin GPIO_PIN_2
#define SCROLL_NO_GPIO_Port GPIOC
#define SDA_BB_Pin GPIO_PIN_9
#define SDA_BB_GPIO_Port GPIOB
#define SCL_BB_Pin GPIO_PIN_8
#define SCL_BB_GPIO_Port GPIOB
#define SCROLL_1_Pin GPIO_PIN_8
#define SCROLL_1_GPIO_Port GPIOD
#define TOF_INT_Pin GPIO_PIN_0
#define TOF_INT_GPIO_Port GPIOE
#define TOF_I2C_RST_Pin GPIO_PIN_1
#define TOF_I2C_RST_GPIO_Port GPIOE
#define UART_RX_Pin GPIO_PIN_1
#define UART_RX_GPIO_Port GPIOA
#define UART_TX_Pin GPIO_PIN_0
#define UART_TX_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
