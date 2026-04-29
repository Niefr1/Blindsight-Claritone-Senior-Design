/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include "i2c_bb.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart4;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_UART4_Init(void);
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * Write 1 byte to a 16-bit-addressed VL53L7CX register.
 * Returns true on success (all ACKs received).
 */
static bool tof_write_reg(uint16_t reg, uint8_t val)
{
    i2c_bb_start();
    if (!i2c_bb_write_byte(0x52))                  { i2c_bb_stop(); return false; }  // device addr + W
    if (!i2c_bb_write_byte((reg >> 8) & 0xFF))     { i2c_bb_stop(); return false; }  // INDEX[15:8]
    if (!i2c_bb_write_byte(reg & 0xFF))            { i2c_bb_stop(); return false; }  // INDEX[7:0]
    if (!i2c_bb_write_byte(val))                   { i2c_bb_stop(); return false; }  // DATA
    i2c_bb_stop();
    return true;
}

/**
 * Read 1 byte from a 16-bit-addressed VL53L7CX register using repeated start.
 * Returns true on success. Byte is returned via *val.
 */
static bool tof_read_reg(uint16_t reg, uint8_t *val)
{
    /* Phase 1: set register pointer via write transaction */
    i2c_bb_start();
    if (!i2c_bb_write_byte(0x52))                  { i2c_bb_stop(); return false; }
    if (!i2c_bb_write_byte((reg >> 8) & 0xFF))     { i2c_bb_stop(); return false; }
    if (!i2c_bb_write_byte(reg & 0xFF))            { i2c_bb_stop(); return false; }

    /* Phase 2: repeated start, device addr + R, read one byte with NACK */
    i2c_bb_start();   /* repeated start (no intervening STOP) */
    if (!i2c_bb_write_byte(0x53))                  { i2c_bb_stop(); return false; }  // device addr + R
    *val = i2c_bb_read_byte(false);  /* NACK = last byte */
    i2c_bb_stop();
    return true;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_UART4_Init();
  SystemIsolation_Config();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* Pin state initialization is done by MX_GPIO_Init():
   *   TOF_LPN = LOW (sensor off)
   *   TOF_I2C_RST = LOW (I2C block normal)
   *   SCL_BB = indeterminate (open-drain released = floats high via pull-up)
   *   SDA_BB = indeterminate (same)
   */

  const char *banner = "\r\n=== L3.3: VL53L7CX device ID read ===\r\n";
  HAL_UART_Transmit(&huart4, (uint8_t *)banner, strlen(banner), HAL_MAX_DELAY);

  /* Initialize bit-bang I2C */
  i2c_bb_init();

  /* Sensor power-up sequence (same as L3.2) */
  HAL_GPIO_WritePin(TOF_I2C_RST_GPIO_Port, TOF_I2C_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(TOF_I2C_RST_GPIO_Port, TOF_I2C_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(2);
  HAL_GPIO_WritePin(TOF_LPN_GPIO_Port, TOF_LPN_Pin, GPIO_PIN_SET);
  HAL_Delay(5);

  uint32_t iter = 0;


  while (1)
  {
	  iter++;
	      char buf[128];
	      int len;

	      /* Select register page 0 */
	      bool ok_page = tof_write_reg(0x7FFF, 0x00);

	      /* Read device_id from 0x0000 and revision_id from 0x0001 */
	      uint8_t device_id = 0xAA;   /* sentinel - should get overwritten */
	      uint8_t revision_id = 0xAA;
	      bool ok_dev = tof_read_reg(0x0000, &device_id);
	      bool ok_rev = tof_read_reg(0x0001, &revision_id);

	      /* Restore default page (good practice, though not strictly needed for test) */
	      tof_write_reg(0x7FFF, 0x02);

	      /* Report */
	      len = snprintf(buf, sizeof(buf),
	          "iter=%lu | page_sel=%s | dev=0x%02X (%s) | rev=0x%02X (%s)\r\n",
	          (unsigned long)iter,
	          ok_page ? "OK" : "FAIL",
	          device_id,   (ok_dev && device_id == 0xF0)   ? "MATCH" : "mismatch",
	          revision_id, (ok_rev && revision_id == 0x0C) ? "MATCH" : "mismatch");
	      HAL_UART_Transmit(&huart4, (uint8_t *)buf, len, HAL_MAX_DELAY);

	      HAL_GPIO_TogglePin(SCROLL_1_GPIO_Port, SCROLL_1_Pin);
	      HAL_Delay(2000);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief RIF Initialization Function
  * @param None
  * @retval None
  */
  static void SystemIsolation_Config(void)
{

  /* USER CODE BEGIN RIF_Init 0 */

  /* USER CODE END RIF_Init 0 */

  /* set all required IPs as secure privileged */
  __HAL_RCC_RIFSC_CLK_ENABLE();

  /* RIF-Aware IPs Config */

  /* set up GPIO configuration */
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);

  /* USER CODE BEGIN RIF_Init 1 */

  /* USER CODE END RIF_Init 1 */
  /* USER CODE BEGIN RIF_Init 2 */

  /* USER CODE END RIF_Init 2 */

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, TOF_LPN_Pin|SCROLL_1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SDA_BB_Pin|SCL_BB_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, TOF_INT_Pin|TOF_I2C_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : TOF_LPN_Pin SCROLL_1_Pin */
  GPIO_InitStruct.Pin = TOF_LPN_Pin|SCROLL_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : SDA_BB_Pin SCL_BB_Pin */
  GPIO_InitStruct.Pin = SDA_BB_Pin|SCL_BB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : TOF_INT_Pin TOF_I2C_RST_Pin */
  GPIO_InitStruct.Pin = TOF_INT_Pin|TOF_I2C_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
