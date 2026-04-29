/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Claritone — Combined L0–L7 program
  *
  * Integrates:
  *   L0  Blinky        — heartbeat LED on SCROLL_1 (PD8)
  *   L1  UART          — UART4 @ 115200 baud status prints
  *   L2  GPIO          — SCL_BB / SDA_BB auxiliary toggling
  *   L3  I2C bit-bang  — VL53L7CX device-ID verification on boot
  *   L4  ULD           — VL53L7CX ranging (non-blocking poll in main loop)
  *   L5  ToF @ HSE     — same ranging path, clock already 266 MHz
  *   L6  HSE           — SystemClock_Config: HSE + PLL1 → 266.67 MHz CPU
  *   L7  Audio         — SAI1 + GPDMA1 ping-pong DMA, 440 Hz sine tone
  *
  * Architecture notes
  * ------------------
  *  • Audio is fully interrupt-driven (HAL_SAI_TxHalfCpltCallback /
  *    HAL_SAI_TxCpltCallback).  The main loop never blocks for audio.
  *  • ToF ranging uses claritone_tof_test_init() + a non-blocking
  *    claritone_tof_poll() wrapper (see Section 4 below).  If your
  *    claritone_tof_test.c only exposes claritone_tof_test_ranging_loop()
  *    you must add a single-shot poll function — see the stub provided.
  *  • The main loop runs on a 100 ms HAL_Delay tick with sub-tick counters
  *    for each periodic task (heartbeat, UART print, ToF poll).
  *
  * Required CubeMX peripheral configuration (combined project)
  * -----------------------------------------------------------
  *   UART4   PA0 TX / PA1 RX,  115200 8N1
  *   SAI1    Block A, master TX, I2S-standard, 32-bit stereo, 48 kHz
  *   GPDMA1  Channel 0 → SAI1_A (circular, word width)
  *   GPIO    PD1  TOF_LPN   OUT PP
  *           PD8  SCROLL_1  OUT PP  (heartbeat LED)
  *           PB1  SD_MODE   OUT PP  (amp enable — set HIGH)
  *           PB8  SCL_BB    OUT OD  (bit-bang I2C clock)
  *           PB9  SDA_BB    OUT OD  (bit-bang I2C data)
  *           PE0  TOF_INT   OUT PP
  *           PE1  TOF_I2C_RST OUT PP
  *
  * External dependencies (bring from your lesson projects)
  *   i2c_bb.h / i2c_bb.c
  *   claritone_tof_test.h / claritone_tof_test.c  — must expose
  *       claritone_tof_test_init()  → int  (0 = OK)
  *       claritone_tof_poll()       → int  (0 = new result ready, <0 = err)
  *       claritone_tof_last_mm()    → uint32_t  (last distance, mm)
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpdma.h"
#include "sai.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "i2c_bb.h"
#include "claritone_tof_test.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HALF_FRAMES     256u
#define TOTAL_FRAMES    (2u * HALF_FRAMES)

#define SINE_LUT_SIZE   256u   /* must be power-of-2, one full period */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */
extern DMA_HandleTypeDef handle_GPDMA1_Channel0 ;

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* --- Audio buffers & LUT ------------------------------------------------- */
static int32_t  audioBuffer[2u * TOTAL_FRAMES]; /* stereo, ping-pong */
static int32_t  sine_lut[SINE_LUT_SIZE];

/* Q16.16 fixed-point phase accumulator — updated only in SAI callbacks */
static uint32_t s_phase_q     = 0u;
static uint32_t s_phase_inc_q = 0u;

/* Diagnostic counters (volatile so debugger/UART reads are coherent) */
static volatile uint32_t audio_half_count = 0u;
static volatile uint32_t audio_full_count = 0u;
static volatile uint32_t sai_err_code     = 0u;
static volatile uint32_t sai_err_count    = 0u;

/* --- ToF state ------------------------------------------------------------ */
static bool     tof_ready      = false;
static uint32_t tof_last_mm    = 0u;
static uint32_t tof_err_count  = 0u;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_GPDMA1_Init();
  MX_UART4_Init();
  MX_SAI1_Init();
  SystemIsolation_Config();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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

  /* set up GPDMA configuration */
  /* set GPDMA1 channel 0 used by SAI1 */
  if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel0,DMA_CHANNEL_SEC|DMA_CHANNEL_PRIV|DMA_CHANNEL_SRC_SEC|DMA_CHANNEL_DEST_SEC)!= HAL_OK )
  {
    Error_Handler();
  }

  /* set up GPIO configuration */
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOB,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);

/* USER CODE BEGIN RIF_Init 1 */

/* USER CODE END RIF_Init 1 */
/* USER CODE BEGIN RIF_Init 2 */

/* USER CODE END RIF_Init 2 */

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
