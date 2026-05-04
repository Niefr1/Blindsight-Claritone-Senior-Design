/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Claritone L7 Audio + Front ToF + Sensitivity button
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
#include <math.h>
#include <stdio.h>
#include "tof_front.h"
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
extern DMA_HandleTypeDef handle_GPDMA1_Channel0 ;

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static volatile int16_t g_user_volume_q15 = 32767;

static volatile uint8_t g_muted = 0;

typedef enum {
    TONE_PURE = 0,    /* 220-1320 Hz */
    TONE_LOW,         /* 110-660 Hz  */
    TONE_HIGH,        /* 440-2640 Hz */
    TONE_COUNT
} tone_preset_t;

static const char *tone_name[TONE_COUNT] = { "PURE", "LOW", "HIGH" };
static volatile tone_preset_t g_tone_preset = TONE_PURE;

typedef enum {
    SENS_CLOSE = 0,
    SENS_MEDIUM,
    SENS_FAR,
    SENS_COUNT
} sensitivity_mode_t;

static const uint16_t sens_max_mm[SENS_COUNT] = { 300, 800, 1500 };
static const char    *sens_name[SENS_COUNT]   = { "CLOSE", "MEDIUM", "FAR" };

static volatile sensitivity_mode_t g_sens_mode = SENS_CLOSE;

#define HALF_FRAMES   128
#define TOTAL_FRAMES  (2 * HALF_FRAMES)
static int32_t audioBuffer[2 * TOTAL_FRAMES];

volatile int16_t g_volume_q15 = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */
static void sine_lut_init(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint8_t btn_tone_check_press(void)
{
    static uint8_t debounce_count = 0;
    static uint8_t armed = 1;

    uint8_t raw = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4) == GPIO_PIN_RESET) ? 0 : 1;

    if (raw == 0) {
        if (debounce_count < 3) debounce_count++;
    } else {
        debounce_count = 0;
        armed = 1;
    }

    if (debounce_count >= 3 && armed) {
        armed = 0;
        return 1;
    }
    return 0;
}

static int8_t scroll_wheel_poll(void)
{
    static uint8_t last_a = 1;
    static uint8_t last_b = 1;
    uint8_t a = (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_8) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t b = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_1) == GPIO_PIN_SET) ? 1 : 0;

    int8_t result = 0;
    /* Falling edge on A = +1 (right), falling edge on B = -1 (left) */
    if (a == 0 && last_a == 1)      result = +1;
    else if (b == 0 && last_b == 1) result = -1;

    last_a = a;
    last_b = b;
    return result;
}

static uint8_t scroll_button_check_press(void)
{
    static uint8_t debounce_count = 0;
    static uint8_t armed = 1;

    uint8_t raw = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_2) == GPIO_PIN_RESET) ? 0 : 1;

    if (raw == 0) {
        if (debounce_count < 3) debounce_count++;
    } else {
        debounce_count = 0;
        armed = 1;
    }

    if (debounce_count >= 3 && armed) {
        armed = 0;
        return 1;
    }
    return 0;
}

static uint8_t btn_sens_check_press(void)
{
    static uint8_t debounce_count = 0;
    static uint8_t armed = 1;

    uint8_t raw = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6) == GPIO_PIN_RESET) ? 0 : 1;

    if (raw == 0) {
        if (debounce_count < 3) debounce_count++;
    } else {
        debounce_count = 0;
        armed = 1;
    }

    if (debounce_count >= 3 && armed) {
        armed = 0;
        return 1;
    }
    return 0;
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
  SCB_DisableDCache();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_GPDMA1_Init();
  MX_UART4_Init();
  MX_SAI1_Init();
  SystemIsolation_Config();
  /* USER CODE BEGIN 2 */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef g = {0};
  g.Pin = GPIO_PIN_1;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &g);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

  sine_lut_init();

  /* Boot banner --- multi-section, each ≤96 bytes for stack safety */
    {
        const char *line;

        line = "\r\n"
               "==========================================\r\n";
        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);

        line = "  Claritone\r\n"
               "  Wearable Navigation for the Visually Impaired\r\n";
        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);

        line = "  ESE441 Senior Design  |  Spring 2026\r\n"
               "  Stony Brook University\r\n";
        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);

        line = "==========================================\r\n\r\n"
               "System initialization:\r\n";
        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);

        line = "  [OK] HAL initialized\r\n"
               "  [OK] GPDMA1 channel 0 (SAI DMA)\r\n"
               "  [OK] UART4 (115200 8N1)\r\n"
               "  [OK] SAI1 (48 kHz I2S stereo)\r\n";
        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);

        line = "  [OK] GPIO security (RIF)\r\n"
               "  [OK] Audio amplifier enabled (PB1)\r\n"
               "  [OK] Sine LUT (256 points)\r\n";
        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);
    }

  memset(audioBuffer, 0, sizeof(audioBuffer));
  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)audioBuffer, 2*TOTAL_FRAMES) != HAL_OK)
  {
      Error_Handler();
  }

  {
        const char *line = "  Initializing front ToF (~10 sec)...\r\n";
        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);
    }

    /* Start audio */

  uint8_t tof_status = ToF_Front_Init();
  if (tof_status != 0) {
      char msg[48];
      int len = snprintf(msg, sizeof(msg), "ToF init FAILED (code=%u)\r\n", tof_status);
      HAL_UART_Transmit(&huart4, (const uint8_t *)msg, len, HAL_MAX_DELAY);
  } else {
	  const char *line;
	        line = "  [OK] Front ToF (VL53L7CX, 4x4 grid, 15 Hz)\r\n";
	        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);

	        line = "------------------------------------------\r\n"
	               "  Audio   : 220-1320 Hz tone, distance modulated\r\n";
	        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);

	        line = "  Pitch   : closer = higher\r\n"
	               "  Rhythm  : closer = faster pulse\r\n"
	               "  Volume  : closer = louder\r\n";
	        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);

	        line = "  Controls: BTN_SENS (PC6) cycles sensitivity\r\n"
	               "            BTN_TONE (PC4) cycles tone preset\r\n";
	        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);

	        line = "            Scroll wheel sets master volume\r\n"
	               "            Wheel press toggles mute\r\n"
	               "==========================================\r\n"
	               "  Ready. Wave hand near sensor for demo.\r\n"
	               "==========================================\r\n\r\n";
	        HAL_UART_Transmit(&huart4, (const uint8_t *)line, strlen(line), HAL_MAX_DELAY);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (btn_sens_check_press()) {
        g_sens_mode = (g_sens_mode + 1) % SENS_COUNT;
        char msg[64];
        int len = snprintf(msg, sizeof(msg),
            ">>> Sensitivity: %s (max %u mm)\r\n",
            sens_name[g_sens_mode], sens_max_mm[g_sens_mode]);
        HAL_UART_Transmit(&huart4, (const uint8_t *)msg, len, 10);
    }

    if (btn_tone_check_press()) {
            g_tone_preset = (g_tone_preset + 1) % TONE_COUNT;
            char msg[48];
            int len = snprintf(msg, sizeof(msg),
                ">>> Tone: %s\r\n", tone_name[g_tone_preset]);
            HAL_UART_Transmit(&huart4, (const uint8_t *)msg, len, 10);
        }

    int8_t scroll = scroll_wheel_poll();
        if (scroll != 0) {
            int32_t vu = g_user_volume_q15 + (scroll * 4096);
            if (vu < 0)     vu = 0;
            if (vu > 32767) vu = 32767;
            g_user_volume_q15 = (int16_t)vu;

            char msg[48];
            int len = snprintf(msg, sizeof(msg),
                ">>> Volume: %d/32767\r\n", (int)g_user_volume_q15);
            HAL_UART_Transmit(&huart4, (const uint8_t *)msg, len, 10);
        }

        /* NEW: mute button check goes here */
           if (scroll_button_check_press()) {
               g_muted = !g_muted;
               const char *m = g_muted ? ">>> MUTED\r\n" : ">>> UNMUTED\r\n";
               HAL_UART_Transmit(&huart4, (const uint8_t *)m, strlen(m), 10);
           }

    /* These two persist across iterations -- declared static here for locality. */
       static int32_t  s_target_vol     = 0;       /* volume during ON half of beat */
       static uint32_t s_beat_period_ms = 1000;    /* current beat period (slow default) */

       tof_front_state_t st;
       if (ToF_Front_Poll(&st)) {
           if (st.valid) {
               int32_t d = st.distance_mm;
               int32_t max_mm = sens_max_mm[g_sens_mode];
               int32_t vol;
               if (d < 50)            vol = 32767;
               else if (d > max_mm)   vol = 0;
               else                   vol = (int32_t)(32767L * (max_mm - d) / (max_mm - 50));

               vol = (vol / 2048) * 2048;
                           if (vol > 32767) vol = 32767;

                           /* Apply user volume scale */
                                       vol = (int32_t)(((int64_t)vol * g_user_volume_q15) >> 15);

                                       /* Apply mute */
                                       if (g_muted) vol = 0;

                                       s_target_vol = vol;

               /* Beat period from distance: close = 150ms, far = 700ms.
                * Same vol curve drives it: high vol -> short period (fast pulse). */
               s_beat_period_ms = 700 - ((uint32_t)vol * 550u) / 32767u;

               char msg[80];
               int len = snprintf(msg, sizeof(msg),
                   "ToF: %u mm  vol=%d  beat=%lums  [%s]\r\n",
                   st.distance_mm, (int)vol,
                   (unsigned long)s_beat_period_ms,
                   sens_name[g_sens_mode]);
               HAL_UART_Transmit(&huart4, (const uint8_t *)msg, len, 10);
           } else {
               s_target_vol = 0;
           }
       }

       /* Beat gating -- runs every loop iteration, not just on ToF poll. */
       if (s_target_vol == 0) {
           g_volume_q15 = 0;
       } else {
           uint32_t phase = HAL_GetTick() % s_beat_period_ms;
           if (phase < (s_beat_period_ms / 2)) {
               g_volume_q15 = (int16_t)s_target_vol;
           } else {
               g_volume_q15 = 0;
           }
       }
    HAL_Delay(10);
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
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
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
volatile uint32_t halfCount = 0;
volatile uint32_t fullCount = 0;
volatile uint32_t sai_err_code = 0;
volatile uint32_t sai_err_count = 0;

#define SINE_LUT_SIZE   256u
static int32_t sine_lut[SINE_LUT_SIZE];
static uint32_t s_phase_q = 0;

static void sine_lut_init(void)
{
    const float twoPi = 2.0f * 3.1415926535f;
    for (uint32_t i = 0; i < SINE_LUT_SIZE; ++i) {
        float s = sinf(twoPi * (float)i / (float)SINE_LUT_SIZE);
        sine_lut[i] = (int32_t)(0x7FFFFFFF * 0.9f * s);
    }
    s_phase_q = 0;
}

static void fill_frames(uint32_t word_offset)
{
    uint32_t phase = s_phase_q;
    const uint32_t mask = (SINE_LUT_SIZE - 1u) << 16;
    int32_t vol = g_volume_q15;

    if (vol < 0) vol = 0;
    if (vol > 32767) vol = 32767;

    /* Avoid uint64 in ISR. Precompute scale: SINE_LUT_SIZE * 65536 / 48000 = 256 * 65536 / 48000 = 349.5
     * Use (freq_hz * 349) which gives same result within rounding for our range. */
    /* Per-preset frequency range */
        uint32_t base_hz, range_hz;
        tone_preset_t preset = g_tone_preset;
        if (preset == TONE_LOW)       { base_hz = 110u;  range_hz = 550u;  }
        else if (preset == TONE_HIGH) { base_hz = 440u;  range_hz = 2200u; }
        else                          { base_hz = 220u;  range_hz = 1100u; }

        uint32_t freq_hz = base_hz + ((uint32_t)vol * range_hz) / 32767u;
        uint32_t inc = (freq_hz * 5594u) >> 4;

    for (int i = 0; i < HALF_FRAMES; ++i) {
        uint32_t idx = (phase & mask) >> 16;
        int32_t s = sine_lut[idx];
        int32_t out = (int32_t)(((int64_t)s * vol) >> 15);
        audioBuffer[word_offset + 2*i + 0] = out;
        audioBuffer[word_offset + 2*i + 1] = out;
        phase += inc;
    }

    s_phase_q = phase;
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    (void)hsai;
    halfCount++;
    fill_frames(0);
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
    (void)hsai;
    fullCount++;
    fill_frames(2 * HALF_FRAMES);
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
    (void)hsai;
    sai_err_code = HAL_SAI_GetError(hsai);
    sai_err_count++;
}
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
