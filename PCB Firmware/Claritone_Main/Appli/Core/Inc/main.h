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

/* --- ToF sensor control -------------------------------------------------- */
#define TOF_LPN_Pin           GPIO_PIN_1
#define TOF_LPN_GPIO_Port     GPIOD
#define TOF_INT_Pin           GPIO_PIN_0
#define TOF_INT_GPIO_Port     GPIOE
#define TOF_I2C_RST_Pin       GPIO_PIN_1
#define TOF_I2C_RST_GPIO_Port GPIOE

/* --- I2C bit-bang (VL53L7CX) -------------------------------------------- */
#define SCL_BB_Pin            GPIO_PIN_8
#define SCL_BB_GPIO_Port      GPIOB
#define SDA_BB_Pin            GPIO_PIN_9
#define SDA_BB_GPIO_Port      GPIOB

/* --- Audio amplifier enable ---------------------------------------------- */
#define SD_MODE_Pin           GPIO_PIN_1
#define SD_MODE_GPIO_Port     GPIOB

/* --- UART4 --------------------------------------------------------------- */
#define UART_TX_Pin           GPIO_PIN_0
#define UART_TX_GPIO_Port     GPIOA
#define UART_RX_Pin           GPIO_PIN_1
#define UART_RX_GPIO_Port     GPIOA

/* --- Scroll wheel encoder (thumbwheel) ----------------------------------- */
/* SCROLL_1 (PD8): CLK / channel A — input, pull-up                         */
/* SCROLL_2 (PA9): DT  / channel B — input, pull-up (direction sense)       */
/* SCROLL_NO(PD9): SW  / push-click — input, pull-up (reserved)             */
#define SCROLL_1_Pin          GPIO_PIN_8
#define SCROLL_1_GPIO_Port    GPIOD
#define SCROLL_2_Pin          GPIO_PIN_9
#define SCROLL_2_GPIO_Port    GPIOA
#define SCROLL_NO_Pin         GPIO_PIN_9
#define SCROLL_NO_GPIO_Port   GPIOD

/* --- Pushbuttons (active-low, pull-up) ----------------------------------- */
/* BTN_TONE (PC4): cycle tone preset                                         */
/* BTN_SENS (PC6): cycle sensitivity level                                   */
#define BTN_TONE_Pin          GPIO_PIN_4
#define BTN_TONE_GPIO_Port    GPIOC
#define BTN_SENS_Pin          GPIO_PIN_6
#define BTN_SENS_GPIO_Port    GPIOC

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
