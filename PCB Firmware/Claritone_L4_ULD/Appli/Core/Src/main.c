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
#include "claritone_tof_test.h"
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

static void i2c_bb_timing_diagnostic(void)
{
    char buf[128];
    int len;

    /* Baseline: SystemCoreClock value */
    len = snprintf(buf, sizeof(buf),
        "\r\n--- I2C bit-bang timing diagnostic ---\r\n"
        "SystemCoreClock = %lu Hz\r\n",
        (unsigned long)SystemCoreClock);
    HAL_UART_Transmit(&huart4, (const uint8_t *)buf, len, HAL_MAX_DELAY);

    /* Ensure DWT is ticking */
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
        len = snprintf(buf, sizeof(buf), "WARNING: DWT not enabled; enabling now\r\n");
        HAL_UART_Transmit(&huart4, (const uint8_t *)buf, len, HAL_MAX_DELAY);
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }

    /* --- Measurement 1: cost of dwt_delay_us(1) --- */
    {
        extern void i2c_bb_init(void);   /* make sure DWT init ran */
        i2c_bb_init();

        uint32_t start = DWT->CYCCNT;
        for (int i = 0; i < 1000; i++) {
            /* Call the exact same delay used in the bit-bang loop.
             * We can't call the static function directly, so we have to
             * move this measurement into i2c_bb.c. For now, do a small
             * proxy: bodywise identical code. */
            uint32_t t0 = DWT->CYCCNT;
            uint32_t target = 1 * (SystemCoreClock / 1000000u);
            while ((DWT->CYCCNT - t0) < target) { /* spin */ }
        }
        uint32_t end = DWT->CYCCNT;
        uint32_t cycles_per_call = (end - start) / 1000u;

        len = snprintf(buf, sizeof(buf),
            "dwt_delay_us(1) measured: %lu cycles/call = %lu ns\r\n",
            (unsigned long)cycles_per_call,
            (unsigned long)((cycles_per_call * 1000u) / (SystemCoreClock / 1000000u)));
        HAL_UART_Transmit(&huart4, (const uint8_t *)buf, len, HAL_MAX_DELAY);
    }

    /* --- Measurement 2: cost of one i2c_bb_write_byte --- */
    /* This measures a REAL write-byte call, including 18 half-bit delays
     * (8 data bits × 2 half-bits + 1 ACK × 2 half-bits).
     * The sensor NACKs because there's no active transaction, but timing is
     * the same. */
    {
        uint32_t start = DWT->CYCCNT;
        i2c_bb_start();
        for (int i = 0; i < 100; i++) {
            (void)i2c_bb_write_byte(0x00);
        }
        i2c_bb_stop();
        uint32_t end = DWT->CYCCNT;
        uint32_t cycles_per_byte = (end - start) / 100u;

        len = snprintf(buf, sizeof(buf),
            "i2c_bb_write_byte measured: %lu cycles/byte = %lu us\r\n"
            "  effective bit-rate: %lu bits/sec (%lu kHz)\r\n",
            (unsigned long)cycles_per_byte,
            (unsigned long)(cycles_per_byte / (SystemCoreClock / 1000000u)),
            /* 9 bits per byte (8 data + 1 ACK) */
            (unsigned long)(9ul * (SystemCoreClock / cycles_per_byte)),
            (unsigned long)((9ul * (SystemCoreClock / cycles_per_byte)) / 1000u));
        HAL_UART_Transmit(&huart4, (const uint8_t *)buf, len, HAL_MAX_DELAY);
    }

    /* --- Projection: how long to transmit 85 KB at this rate? --- */
    {
        extern uint32_t SystemCoreClock;
        /* From measurement 2, recompute: */
        uint32_t start = DWT->CYCCNT;
        i2c_bb_start();
        for (int i = 0; i < 100; i++) {
            (void)i2c_bb_write_byte(0x00);
        }
        i2c_bb_stop();
        uint32_t end = DWT->CYCCNT;
        uint32_t cycles_per_byte = (end - start) / 100u;

        /* 85 KB of firmware = ~85000 bytes, but each ULD WrMulti call has
         * ~3 bytes of addressing overhead (address + 2-byte index).
         * Assume 85000 + overhead ≈ 86000 bytes total. */
        uint32_t total_cycles = 86000ul * cycles_per_byte;
        uint32_t total_ms = total_cycles / (SystemCoreClock / 1000u);

        len = snprintf(buf, sizeof(buf),
            "Projected 85 KB transfer: %lu ms\r\n"
            "--- end diagnostic ---\r\n\r\n",
            (unsigned long)total_ms);
        HAL_UART_Transmit(&huart4, (const uint8_t *)buf, len, HAL_MAX_DELAY);
    }
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
  /* USER CODE BEGIN WHILE */

  /* Banner */
  const char *banner = "\r\n=== L4.0: VL53L7CX ULD integration ===\r\n";
  HAL_UART_Transmit(&huart4, (const uint8_t *)banner, strlen(banner), HAL_MAX_DELAY);

  /* Initialize and configure the sensor */
  if (claritone_tof_test_init() != 0) {
      const char *fail = "Init failed. Halting.\r\n";
      while (1) {
          HAL_UART_Transmit(&huart4, (const uint8_t *)fail, strlen(fail), HAL_MAX_DELAY);
          HAL_GPIO_TogglePin(SCROLL_1_GPIO_Port, SCROLL_1_Pin);
          HAL_Delay(1000);
      }
  }

  /* Never returns */
  claritone_tof_test_ranging_loop();

  /* Should never reach here */
  while (1)
  {
      HAL_Delay(1000);
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
