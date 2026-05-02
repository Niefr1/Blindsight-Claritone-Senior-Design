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
#include "csi.h"
#include "dcmipp.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
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

/* USER CODE BEGIN PV */

#define CAM_WIDTH   640
#define CAM_HEIGHT  400
#define CAM_SIZE (CAM_WIDTH * CAM_HEIGHT)  // = 640*400 = 256000
// Remove the camera_frame array declaration entirely
// Replace with a pointer to the known noncacheable address
#define camera_frame ((uint8_t*)0x34080100)


volatile HAL_StatusTypeDef cap_status;
volatile HAL_StatusTypeDef frame_counter_status_before;
volatile HAL_StatusTypeDef frame_counter_status_after;
volatile HAL_StatusTypeDef data_counter_status_after;

volatile uint32_t frame_counter_before = 0;
volatile uint32_t frame_counter_after  = 0;
volatile uint32_t data_counter_after   = 0;
volatile uint32_t dcmipp_error         = 0;

volatile uint8_t sample0    = 0;
volatile uint8_t sample100  = 0;
volatile uint8_t sample1000 = 0;

volatile uint32_t frame_counter = 0;
volatile uint32_t data_counter = 0;
volatile HAL_StatusTypeDef frame_counter_status;
volatile HAL_StatusTypeDef data_counter_status;

volatile uint32_t csi_sr0_dbg   = 0;
volatile uint32_t csi_sr1_dbg   = 0;
volatile uint32_t csi_err1_dbg  = 0;
volatile uint32_t csi_err2_dbg  = 0;
volatile uint32_t csi_spdfr_dbg = 0;

volatile uint32_t dcmipp_cmsr1_dbg = 0;
volatile uint32_t dcmipp_cmsr2_dbg = 0;
volatile uint32_t dcmipp_p2sr_dbg  = 0;

volatile uint32_t dcmipp_cmcr_dbg = 0;
volatile uint32_t dcmipp_p2fscr_dbg = 0;
volatile uint32_t dcmipp_p2ppcr_dbg = 0;
volatile uint32_t dcmipp_p2ppm0pr_dbg = 0;
volatile uint32_t csi_sr0_eof = 0;



/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */




uint8_t OV9281_ADDR = (0x60 << 1); // adjust if needed

HAL_StatusTypeDef OV_WriteReg(uint16_t reg, uint8_t val) {
	uint8_t data[3];

	data[0] = (reg >> 8) & 0xFF;
	data[1] = reg & 0xFF;
	data[2] = val;

	return HAL_I2C_Master_Transmit(&hi2c2, OV9281_ADDR, data, 3, 100);
}

void OV9281_Init(void)
{
    OV_WriteReg(0x0103, 0x01);
    HAL_Delay(20);

    OV_WriteReg(0x0100, 0x00);
    HAL_Delay(5);

    // === EXACT 640x400 TABLE ===

    OV_WriteReg(0x0302, 0x32);
    OV_WriteReg(0x030d, 0x60);
    OV_WriteReg(0x030e, 0x02);

    OV_WriteReg(0x3001, 0x00);
    OV_WriteReg(0x3004, 0x00);
    OV_WriteReg(0x3005, 0x00);
    OV_WriteReg(0x3006, 0x04);
    OV_WriteReg(0x3011, 0x0a);
    OV_WriteReg(0x3013, 0x18);
    OV_WriteReg(0x3022, 0x01);

    OV_WriteReg(0x3030, 0x04);  // 🔥 FIXED

    OV_WriteReg(0x3039, 0x32);
    OV_WriteReg(0x303a, 0x00);

    OV_WriteReg(0x3500, 0x00);
    OV_WriteReg(0x3501, 0x01);
    OV_WriteReg(0x3502, 0xf4);
    OV_WriteReg(0x3503, 0x08);
    OV_WriteReg(0x3505, 0x8c);
    OV_WriteReg(0x3507, 0x03);
    OV_WriteReg(0x3508, 0x00);
    OV_WriteReg(0x3509, 0x10);

    OV_WriteReg(0x3610, 0x80);
    OV_WriteReg(0x3611, 0xa0);
    OV_WriteReg(0x3620, 0x6f);  // 🔥 FIXED
    OV_WriteReg(0x3632, 0x56);
    OV_WriteReg(0x3633, 0x78);
    OV_WriteReg(0x3662, 0x07);
    OV_WriteReg(0x3666, 0x00);
    OV_WriteReg(0x366f, 0x5a);
    OV_WriteReg(0x3680, 0x84);

    OV_WriteReg(0x3712, 0x80);
    OV_WriteReg(0x372d, 0x22);
    OV_WriteReg(0x3731, 0x80);
    OV_WriteReg(0x3732, 0x30);

    OV_WriteReg(0x3778, 0x10);
    OV_WriteReg(0x377d, 0x22);
    OV_WriteReg(0x3788, 0x02);
    OV_WriteReg(0x3789, 0xa4);
    OV_WriteReg(0x378a, 0x00);
    OV_WriteReg(0x378b, 0x4a);
    OV_WriteReg(0x3799, 0x20);

    OV_WriteReg(0x3800, 0x00);
    OV_WriteReg(0x3801, 0x00);
    OV_WriteReg(0x3802, 0x00);
    OV_WriteReg(0x3803, 0x00);
    OV_WriteReg(0x3804, 0x05);
    OV_WriteReg(0x3805, 0x0f);
    OV_WriteReg(0x3806, 0x03);
    OV_WriteReg(0x3807, 0x2f);

    OV_WriteReg(0x3808, 0x02);
    OV_WriteReg(0x3809, 0x80);
    OV_WriteReg(0x380a, 0x01);
    OV_WriteReg(0x380b, 0x90);

    OV_WriteReg(0x380c, 0x02);
    OV_WriteReg(0x380d, 0xd8);
    OV_WriteReg(0x380e, 0x02);
    OV_WriteReg(0x380f, 0x08);

    OV_WriteReg(0x3810, 0x00);
    OV_WriteReg(0x3811, 0x04);
    OV_WriteReg(0x3812, 0x00);
    OV_WriteReg(0x3813, 0x04);
    OV_WriteReg(0x3814, 0x31);
    OV_WriteReg(0x3815, 0x22);

    OV_WriteReg(0x3820, 0x60);
    OV_WriteReg(0x3821, 0x01);

    OV_WriteReg(0x4003, 0x40);
    OV_WriteReg(0x4008, 0x02);
    OV_WriteReg(0x4009, 0x05);
    OV_WriteReg(0x400c, 0x00);
    OV_WriteReg(0x400d, 0x03);

    OV_WriteReg(0x4010, 0x40);
    OV_WriteReg(0x4043, 0x40);

    OV_WriteReg(0x4307, 0x30);
    OV_WriteReg(0x4317, 0x00);

    OV_WriteReg(0x4501, 0x00);
    OV_WriteReg(0x4507, 0x03);
    OV_WriteReg(0x4509, 0x80);
    OV_WriteReg(0x450a, 0x08);

    OV_WriteReg(0x4601, 0x04);
    OV_WriteReg(0x470f, 0x00);
    OV_WriteReg(0x4f07, 0x00);


    OV_WriteReg(0x5000, 0x9f);
    OV_WriteReg(0x5001, 0x00);
    OV_WriteReg(0x5e00, 0x00);

    OV_WriteReg(0x5d00, 0x07);
    OV_WriteReg(0x5d01, 0x00);

    // Force all sync packets to VC0
    OV_WriteReg(0x4837, 0x00);  // MIPI global timing
    OV_WriteReg(0x3666, 0x00);  // already set, keep
    OV_WriteReg(0x4800, 0x04);  // enable frame sync on VC0 only

    HAL_Delay(20);
}



HAL_StatusTypeDef OV_ReadReg(uint16_t reg, uint8_t *val) {
	uint8_t addr[2];

	addr[0] = (reg >> 8) & 0xFF;
	addr[1] = reg & 0xFF;

	if (HAL_I2C_Master_Transmit(&hi2c2, OV9281_ADDR, addr, 2, 100) != HAL_OK)
		return HAL_ERROR;

	if (HAL_I2C_Master_Receive(&hi2c2, OV9281_ADDR, val, 1, 100) != HAL_OK)
		return HAL_ERROR;

	return HAL_OK;
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
  SystemIsolation_Config(); // ← RIF first
  MX_I2C2_Init();
  MX_DCMIPP_Init();         // ← then peripherals
  /* USER CODE BEGIN 2 */

  uint8_t high = 0, low = 0;
  uint8_t stream_reg = 0;

  /* enable camera module pins */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_2, GPIO_PIN_SET); // POWER_EN
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_3, GPIO_PIN_SET); // LED_EN
  HAL_Delay(100);

  /* camera ID */
  OV_ReadReg(0x300A, &high);
  OV_ReadReg(0x300B, &low);
  volatile uint16_t camera_id = ((uint16_t)high << 8) | low;

  /* stop + init + start camera */
  OV_WriteReg(0x0100, 0x00);
  HAL_Delay(10);

  OV9281_Init();

  OV_WriteReg(0x0100, 0x01);
  HAL_Delay(200);

  OV_ReadReg(0x0100, &stream_reg);
  volatile uint8_t stream_status = stream_reg;

  /* clear frame buffer */
  for (uint32_t i = 0; i < CAM_SIZE; i++)
  {
      camera_frame[i] = 0xAA;
  }

  /* clear old DCMIPP/CSI flags */
  CSI->FCR0 = 0xFFFFFFFF;
  CSI->FCR1 = 0xFFFFFFFF;
  DCMIPP->CMFCR = 0xFFFFFFFF;
  DCMIPP->P2FCR = 0xFFFFFFFF;

  MODIFY_REG(DCMIPP->CMCR, DCMIPP_CMCR_PSFC, (2U << DCMIPP_CMCR_PSFC_Pos));

  /* counters before */
  frame_counter_status_before =
      HAL_DCMIPP_PIPE_ReadFrameCounter(&hdcmipp, DCMIPP_PIPE2, (uint32_t *)&frame_counter_before);

  /* start capture into camera_frame */
  // Start Pipe0 first to enable routing
  HAL_DCMIPP_CSI_PIPE_Start(&hdcmipp, DCMIPP_PIPE0,
      DCMIPP_VIRTUAL_CHANNEL0,
      (uint32_t)0x34080000,  // dummy address, pipe0 data discarded
      DCMIPP_MODE_CONTINUOUS);

  // Then start Pipe2 for actual capture
  cap_status = HAL_DCMIPP_CSI_PIPE_Start(&hdcmipp, DCMIPP_PIPE2,
      DCMIPP_VIRTUAL_CHANNEL0,
      (uint32_t)camera_frame,
      DCMIPP_MODE_CONTINUOUS);



  // HAL overwrites PFCR during start, fix HSFR after
  MODIFY_REG(CSI->PFCR, CSI_PFCR_HSFR, (0x59U << CSI_PFCR_HSFR_Pos));

  // Reset PHY to lock onto correct frequency
  CSI->PRCR = 0x00;          // assert reset
  HAL_Delay(2);
  CSI->PRCR = CSI_PRCR_PEN;  // release reset
  HAL_Delay(50);              // wait for PHY lock at 800Mbps

  HAL_Delay(1000);  // your existing capture wait

  // Invalidate cache so CPU reads what DMA wrote
  SCB_InvalidateDCache_by_Addr((uint32_t*)camera_frame, CAM_SIZE);

  sample0    = camera_frame[0];
  sample100  = camera_frame[100];
  sample1000 = camera_frame[1000];


  /* counters after */
  frame_counter_status_after =
      HAL_DCMIPP_PIPE_ReadFrameCounter(&hdcmipp, DCMIPP_PIPE2, (uint32_t *)&frame_counter_after);

  data_counter_status_after =
      HAL_DCMIPP_PIPE_GetDataCounter(&hdcmipp, DCMIPP_PIPE2, (uint32_t *)&data_counter_after);

  volatile uint32_t frame_counter_direct = DCMIPP->CMFRCR & 0xFFFF;

  dcmipp_error = HAL_DCMIPP_GetError(&hdcmipp);

  /* sample buffer */
  sample0    = camera_frame[0];
  sample100  = camera_frame[100];
  sample1000 = camera_frame[1000];

  /* final debug */
  frame_counter_status =
      HAL_DCMIPP_PIPE_ReadFrameCounter(&hdcmipp, DCMIPP_PIPE2, (uint32_t *)&frame_counter);

  data_counter_status =
      HAL_DCMIPP_PIPE_GetDataCounter(&hdcmipp, DCMIPP_PIPE2, (uint32_t *)&data_counter);

  csi_sr0_dbg   = CSI->SR0;
  csi_sr1_dbg   = CSI->SR1;
  csi_err1_dbg  = CSI->ERR1;
  csi_err2_dbg  = CSI->ERR2;
  csi_spdfr_dbg = CSI->SPDFR;
  csi_sr0_eof = (CSI->SR0 >> 12) & 0x1; // EOF0F bit

  dcmipp_cmsr1_dbg = DCMIPP->CMSR1;
  dcmipp_cmsr2_dbg = DCMIPP->CMSR2;

  /* use P1 registers because you are using PIPE2 */
  dcmipp_p2sr_dbg      = DCMIPP->P2SR;
  dcmipp_cmcr_dbg      = DCMIPP->CMCR;
  dcmipp_p2fscr_dbg    = DCMIPP->P2FSCR;
  dcmipp_p2ppcr_dbg    = DCMIPP->P2PPCR;
  dcmipp_p2ppm0pr_dbg  = DCMIPP->P2PPM0PR;
  volatile uint32_t csi_pfcr  = CSI->PFCR;   // PHY frequency control
  volatile uint32_t csi_prcr  = CSI->PRCR;   // PHY reset control
  volatile uint32_t csi_pmcr  = CSI->PMCR;   // PHY mode control
  volatile uint32_t csi_ptsr  = CSI->PTSR;   // PHY test status
  volatile uint32_t frame_addr = (uint32_t)camera_frame;
  volatile uint32_t dcmipp_p2sr_raw = DCMIPP->P2SR;
  volatile uint32_t p2sr_full = DCMIPP->P2SR;
  volatile uint32_t p0fscr = DCMIPP->P0FSCR;
  volatile uint32_t p1fscr = DCMIPP->P1FSCR;
  volatile uint32_t p2fscr = DCMIPP->P2FSCR;
  volatile uint32_t risaf_isr = RISAF4->IASR;  // illegal access status
  __NOP(); // breakpoint here


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
    __HAL_RCC_RIFSC_CLK_ENABLE();

    // Single RIMC config for DCMIPP - privileged access
    RIMC_MasterConfig_t RIMC_master = {0};
    RIMC_master.MasterCID = RIF_CID_1;
    RIMC_master.SecPriv   = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);

    // GPIO config
    HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_10, GPIO_PIN_SEC|GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_11, GPIO_PIN_SEC|GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_2,  GPIO_PIN_SEC|GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_3,  GPIO_PIN_SEC|GPIO_PIN_NPRIV);
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
	while (1) {
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
