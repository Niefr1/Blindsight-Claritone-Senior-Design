/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Claritone_Main — complete product firmware
  *
  * Integrates all confirmed-working subsystems:
  *
  *   Clock        HSE 25 MHz + PLL1 → 266.67 MHz CPU (from L6)
  *   UART4        115200 8N1 on PA0/PA1 — boot banner + periodic status
  *   SAI1 / DMA   48 kHz stereo I2S → dual MAX98357A amplifiers
  *   Audio engine claritone_audio — spatial tone, ported from Sola's
  *                playback.cpp: equal-power panning, head shadow, ITD
  *                delay, distance attenuation, per-preset pitch table
  *   UI           claritone_ui — BTN_SENS (PC6), BTN_TONE (PC4),
  *                quadrature scroll wheel (PD8 CLK / PA9 DT / PD9 SW)
  *   ToF stubs    claritone_tof_test — WIP; plugs into audio via
  *                ClaritoneObstacle_t array when ready
  *   I2C bit-bang VL53L7CX device-ID boot check on I2C BB (PB8/PB9)
  *
  * Main loop tick: ~1 ms (HAL_Delay(1) per iteration)
  *   Every  10 ms — UI poll (buttons + scroll wheel)
  *   Every  50 ms — audio spatial parameter update
  *   Every 200 ms — ToF poll (when ULD is ready)
  *   Every   1 s  — UART status line
  *
  * ToF integration (when Claritone_L4_ULD work is complete):
  *   1. Implement claritone_tof_poll() and claritone_tof_last_mm() in
  *      claritone_tof_test.c.
  *   2. In the 50 ms audio update block below, populate the obs[] array
  *      with real sensor readings for each of the 4 sensor directions.
  *   3. Sensor index → direction mapping (fixed in claritone_audio.c):
  *        SENSOR_IDX_FRONT=0  SENSOR_IDX_LEFT=1
  *        SENSOR_IDX_RIGHT=2  SENSOR_IDX_REAR=3
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpdma.h"
#include "sai.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "i2c_bb.h"
#include "claritone_tof_test.h"
#include "../../Audio/claritone_audio.h"
#include "../../UI/claritone_ui.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define UART_STATUS_INTERVAL_MS   1000u
#define AUDIO_UPDATE_INTERVAL_MS    50u
#define UI_POLL_INTERVAL_MS         10u
#define TOF_POLL_INTERVAL_MS       200u
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static volatile uint32_t g_audio_half_count = 0u;
static volatile uint32_t g_audio_full_count = 0u;
static volatile uint32_t g_sai_err_count    = 0u;
static volatile bool     g_sai_err_pending  = false; /* set in ISR, cleared in main */

static bool     g_tof_ready   = false;
static uint32_t g_tof_last_mm = 0u;
static uint32_t g_tof_errs    = 0u;
/* USER CODE END PV */

/* USER CODE BEGIN PFP */
static void SystemClock_Config(void);
static void SystemIsolation_Config(void);
static bool tof_verify_device_id(void);
static bool tof_write_reg(uint16_t reg, uint8_t val);
static bool tof_read_reg(uint16_t reg, uint8_t *val);
/* USER CODE END PFP */


/* ===========================================================================
 * main()
 * =========================================================================*/
int main(void)
{
    /* --------------------------------------------------------------------- */
    /* HAL + clock                                                            */
    /* --------------------------------------------------------------------- */
    HAL_Init();
    SystemClock_Config();
    SCB_DisableDCache();    /* Required for SAI DMA coherency on STM32N6     */

    /* --------------------------------------------------------------------- */
    /* Peripheral init                                                        */
    /* --------------------------------------------------------------------- */
    MX_GPIO_Init();
    MX_GPDMA1_Init();
    MX_UART4_Init();
    MX_SAI1_Init();
    SystemIsolation_Config();

    /* Amp enable — SD_MODE driven HIGH by gpio.c; confirm here */
    HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);

    /* --------------------------------------------------------------------- */
    /* Boot banner                                                            */
    /* --------------------------------------------------------------------- */
    {
        char buf[192];
        int  len = snprintf(buf, sizeof(buf),
            "\r\n=== Claritone Main Firmware ===\r\n"
            "CPU:  %lu MHz\r\n"
            "Audio: SAI1 DMA 48 kHz stereo int32\r\n"
            "UI:   BTN_SENS(PC6) BTN_TONE(PC4)"
            " Encoder(SCROLL1=PD8 / SCROLL2=PA9 / SW=PD9)\r\n\r\n",
            (unsigned long)(SystemCoreClock / 1000000u));
        HAL_UART_Transmit(&huart4, (const uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);
    }

    /* --------------------------------------------------------------------- */
    /* I2C bit-bang + VL53L7CX device-ID check                              */
    /* --------------------------------------------------------------------- */
    i2c_bb_init();

    HAL_GPIO_WritePin(TOF_I2C_RST_GPIO_Port, TOF_I2C_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(TOF_I2C_RST_GPIO_Port, TOF_I2C_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(TOF_LPN_GPIO_Port, TOF_LPN_Pin, GPIO_PIN_SET);
    HAL_Delay(5);

    if (tof_verify_device_id()) {
        HAL_UART_Transmit(&huart4,
            (const uint8_t *)"[OK]   VL53L7CX ID verified (0xF0/0x0C)\r\n",
            41u, HAL_MAX_DELAY);
    } else {
        HAL_UART_Transmit(&huart4,
            (const uint8_t *)"[WARN] VL53L7CX ID mismatch — ranging disabled\r\n",
            48u, HAL_MAX_DELAY);
    }

    /* --------------------------------------------------------------------- */
    /* ToF ULD init (WIP)                                                    */
    /* --------------------------------------------------------------------- */
    {
        const char *msg = "Initialising VL53L7CX ULD...\r\n";
        HAL_UART_Transmit(&huart4, (const uint8_t *)msg,
                          (uint16_t)strlen(msg), HAL_MAX_DELAY);
    }
    if (claritone_tof_test_init() == 0) {
        HAL_UART_Transmit(&huart4,
            (const uint8_t *)"[OK]   ToF ULD ready\r\n", 22u, HAL_MAX_DELAY);
        g_tof_ready = true;
    } else {
        HAL_UART_Transmit(&huart4,
            (const uint8_t *)"[WARN] ToF init failed — demo-sweep mode will activate\r\n",
            56u, HAL_MAX_DELAY);
        g_tof_ready = false;
    }

    /* --------------------------------------------------------------------- */
    /* Spatial audio engine init + SAI DMA start                            */
    /* --------------------------------------------------------------------- */
    claritone_audio_init();

    if (HAL_SAI_Transmit_DMA(&hsai_BlockA1,
                              (uint8_t *)g_audio_dma_buf,
                              CLARITONE_DMA_WORDS) != HAL_OK) {
        Error_Handler();
    }
    HAL_UART_Transmit(&huart4,
        (const uint8_t *)"[OK]   SAI1 DMA audio running\r\n", 31u, HAL_MAX_DELAY);

    /* --------------------------------------------------------------------- */
    /* UI module init                                                         */
    /* --------------------------------------------------------------------- */
    claritone_ui_init();
    HAL_UART_Transmit(&huart4,
        (const uint8_t *)"[OK]   UI ready\r\n\r\n", 19u, HAL_MAX_DELAY);

    /* ==================================================================== */
    /* MAIN LOOP                                                             */
    /* ==================================================================== */

    /*
     * DIAGNOSTIC: disable ToF polling until the main loop is confirmed
     * stable.  When you can see the 1-second status lines and buttons work,
     * remove this override and re-enable ToF.
     */
    g_tof_ready = false;

    uint32_t last_ui_tick    = HAL_GetTick();
    uint32_t last_audio_tick = HAL_GetTick();
    /* Pre-arm: subtract the full interval so the very first iteration
     * prints the status line — confirms the loop is alive on UART.        */
    uint32_t last_uart_tick  = HAL_GetTick() - UART_STATUS_INTERVAL_MS;
    uint32_t last_tof_tick   = HAL_GetTick();
    uint32_t uptime_s        = 0u;

    while (1)
    {
        uint32_t now = HAL_GetTick();

        /* ----------------------------------------------------------------- */
        /* SAI error recovery — handle SAFELY from main context             */
        /* (The ISR only sets a flag; all HAL/UART work happens here.)      */
        /* ----------------------------------------------------------------- */
        if (g_sai_err_pending)
        {
            g_sai_err_pending = false;
            char ebuf[64];
            int  elen = snprintf(ebuf, sizeof(ebuf),
                "[SAI ERR] code=0x%08lX count=%lu\r\n",
                (unsigned long)HAL_SAI_GetError(&hsai_BlockA1),
                (unsigned long)g_sai_err_count);
            HAL_UART_Transmit(&huart4, (const uint8_t *)ebuf, (uint16_t)elen, 20u);
            HAL_SAI_DMAStop(&hsai_BlockA1);
            HAL_SAI_Transmit_DMA(&hsai_BlockA1,
                                 (uint8_t *)g_audio_dma_buf,
                                 CLARITONE_DMA_WORDS);
        }

        /* ----------------------------------------------------------------- */
        /* UI poll — every 10 ms                                             */
        /* ----------------------------------------------------------------- */
        if ((now - last_ui_tick) >= UI_POLL_INTERVAL_MS)
        {
            last_ui_tick = now;

            if (claritone_ui_poll()) {
                const ClaritoneUIState_t *ui = claritone_ui_get_state();
                claritone_audio_set_volume(ui->volume_level);
                claritone_audio_set_tone_preset(ui->tone_preset);

                char buf[80];
                int  len = snprintf(buf, sizeof(buf),
                    "[UI] vol=%u  sens=%u (max %.1fm)  preset=%u\r\n",
                    ui->volume_level,
                    ui->sensitivity,
                    (double)CLARITONE_SENS_DIST[ui->sensitivity],
                    ui->tone_preset);
                HAL_UART_Transmit(&huart4, (const uint8_t *)buf,
                                  (uint16_t)len, 10u);
            }
        }

        /* ----------------------------------------------------------------- */
        /* ToF poll — every 200 ms                                           */
        /* ----------------------------------------------------------------- */
        if (g_tof_ready && (now - last_tof_tick) >= TOF_POLL_INTERVAL_MS)
        {
            last_tof_tick = now;
            int rc = claritone_tof_poll();
            if (rc == 0) {
                g_tof_last_mm = claritone_tof_last_mm();
            } else if (rc < 0) {
                g_tof_errs++;
            }
        }

        /* ----------------------------------------------------------------- */
        /* Audio update — every 50 ms                                        */
        /* Build obstacle descriptors from current ToF readings.             */
        /* When ToF is not ready, all valid=0 → demo sweep activates after  */
        /* DEMO_TIMEOUT_MS (defined in claritone_audio.c, default 5 s).     */
        /* ----------------------------------------------------------------- */
        if ((now - last_audio_tick) >= AUDIO_UPDATE_INTERVAL_MS)
        {
            last_audio_tick = now;

            const ClaritoneUIState_t *ui = claritone_ui_get_state();
            ClaritoneObstacle_t obs[SENSOR_COUNT];
            memset(obs, 0, sizeof(obs));

            if (g_tof_ready && g_tof_last_mm > 0u) {
                float dist_m = (float)g_tof_last_mm / 1000.0f;
                float max_m  = CLARITONE_SENS_DIST[ui->sensitivity];

                if (dist_m <= max_m) {
                    /*
                     * Front sensor — single sensor until multi-sensor
                     * ULD is fully wired up.  When left/right/rear sensors
                     * are active, populate obs[1..3] the same way.
                     */
                    obs[SENSOR_IDX_FRONT].valid      = 1u;
                    obs[SENSOR_IDX_FRONT].distance_m = dist_m;
                    obs[SENSOR_IDX_FRONT].dir_x      = 0.0f;
                    obs[SENSOR_IDX_FRONT].dir_z      = 1.0f;
                }
            }

            claritone_audio_update(obs);
        }

        /* ----------------------------------------------------------------- */
        /* UART status — every 1 s                                           */
        /* ----------------------------------------------------------------- */
        if ((now - last_uart_tick) >= UART_STATUS_INTERVAL_MS)
        {
            last_uart_tick = now;
            uptime_s++;

            const ClaritoneUIState_t *ui = claritone_ui_get_state();
            char buf[200];
            int  len = snprintf(buf, sizeof(buf),
                "[%4lu s] %lu MHz | "
                "sai h=%lu f=%lu err=%lu | "
                "tof %lu mm err=%lu | "
                "vol=%u sens=%u preset=%u\r\n",
                (unsigned long)uptime_s,
                (unsigned long)(SystemCoreClock / 1000000u),
                (unsigned long)g_audio_half_count,
                (unsigned long)g_audio_full_count,
                (unsigned long)g_sai_err_count,
                (unsigned long)g_tof_last_mm,
                (unsigned long)g_tof_errs,
                ui->volume_level,
                ui->sensitivity,
                ui->tone_preset);
            HAL_UART_Transmit(&huart4, (const uint8_t *)buf,
                              (uint16_t)len, 20u);
        }

        HAL_Delay(1);

    } /* while(1) */
}


/* ===========================================================================
 * SAI DMA callbacks — forward to spatial audio engine
 * =========================================================================*/

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
    (void)hsai;
    g_audio_half_count++;
    claritone_audio_fill_half(0u);
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
    (void)hsai;
    g_audio_full_count++;
    /* Second half starts at word offset = HALF_FRAMES × 2 (stereo words) */
    claritone_audio_fill_half(CLARITONE_HALF_FRAMES * 2u);
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
    /*
     * ISR-SAFE: increment counter and set a flag ONLY.
     * Do NOT call HAL functions, UART, or HAL_Delay from ISR context —
     * those can block indefinitely when called from a priority-0 IRQ,
     * starving SysTick and permanently locking the main loop.
     * All recovery work (print + DMA restart) happens in the main loop
     * via g_sai_err_pending.
     */
    (void)hsai;
    g_sai_err_count++;
    g_sai_err_pending = true;
}


/* ===========================================================================
 * Clock configuration (L6 — HSE + PLL1 → 266.67 MHz)
 * =========================================================================*/
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.PLL1.PLLState       = RCC_PLL_ON;
    RCC_OscInitStruct.PLL1.PLLSource      = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL1.PLLM          = 1;
    RCC_OscInitStruct.PLL1.PLLN          = 32;
    RCC_OscInitStruct.PLL1.PLLFractional = 0;
    RCC_OscInitStruct.PLL1.PLLP1         = 1;
    RCC_OscInitStruct.PLL1.PLLP2         = 1;
    RCC_OscInitStruct.PLL2.PLLState       = RCC_PLL_NONE;
    RCC_OscInitStruct.PLL3.PLLState       = RCC_PLL_NONE;
    RCC_OscInitStruct.PLL4.PLLState       = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    RCC_ClkInitStruct.ClockType     = RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_HCLK
                                    | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1
                                    | RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_PCLK5
                                    | RCC_CLOCKTYPE_PCLK4;
    RCC_ClkInitStruct.CPUCLKSource  = RCC_CPUCLKSOURCE_IC1;
    RCC_ClkInitStruct.SYSCLKSource  = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider= RCC_APB1_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider= RCC_APB2_DIV1;
    RCC_ClkInitStruct.APB4CLKDivider= RCC_APB4_DIV1;
    RCC_ClkInitStruct.APB5CLKDivider= RCC_APB5_DIV1;
    RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
    RCC_ClkInitStruct.IC1Selection.ClockDivider   = 3;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK) { Error_Handler(); }
}


/* ===========================================================================
 * RIF / Security isolation
 * =========================================================================*/
static void SystemIsolation_Config(void)
{
    extern DMA_HandleTypeDef handle_GPDMA1_Channel0;

    __HAL_RCC_RIFSC_CLK_ENABLE();

    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel0,
            DMA_CHANNEL_SEC | DMA_CHANNEL_PRIV |
            DMA_CHANNEL_SRC_SEC | DMA_CHANNEL_DEST_SEC) != HAL_OK) {
        Error_Handler();
    }

    /* Output pins */
    HAL_GPIO_ConfigPinAttributes(GPIOA, GPIO_PIN_0, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOA, GPIO_PIN_1, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_0, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_1, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_2, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_6, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_8, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_9, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOD, GPIO_PIN_1, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_0, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_1, GPIO_PIN_SEC | GPIO_PIN_NPRIV);

    /* Input pins — UI controls */
    HAL_GPIO_ConfigPinAttributes(GPIOC, GPIO_PIN_4, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOC, GPIO_PIN_6, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOD, GPIO_PIN_8, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOD, GPIO_PIN_9, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOA, GPIO_PIN_9, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
}


/* ===========================================================================
 * I2C boot-check helpers
 * =========================================================================*/
static bool tof_write_reg(uint16_t reg, uint8_t val)
{
    i2c_bb_start();
    if (!i2c_bb_write_byte(0x52u))               { i2c_bb_stop(); return false; }
    if (!i2c_bb_write_byte((reg >> 8u) & 0xFFu)) { i2c_bb_stop(); return false; }
    if (!i2c_bb_write_byte(reg & 0xFFu))         { i2c_bb_stop(); return false; }
    if (!i2c_bb_write_byte(val))                 { i2c_bb_stop(); return false; }
    i2c_bb_stop();
    return true;
}

static bool tof_read_reg(uint16_t reg, uint8_t *val)
{
    i2c_bb_start();
    if (!i2c_bb_write_byte(0x52u))               { i2c_bb_stop(); return false; }
    if (!i2c_bb_write_byte((reg >> 8u) & 0xFFu)) { i2c_bb_stop(); return false; }
    if (!i2c_bb_write_byte(reg & 0xFFu))         { i2c_bb_stop(); return false; }
    i2c_bb_start();
    if (!i2c_bb_write_byte(0x53u))               { i2c_bb_stop(); return false; }
    *val = i2c_bb_read_byte(false);
    i2c_bb_stop();
    return true;
}

static bool tof_verify_device_id(void)
{
    uint8_t dev = 0u, rev = 0u;
    if (!tof_write_reg(0x7FFFu, 0x00u)) return false;
    if (!tof_read_reg(0x0000u, &dev))   return false;
    if (!tof_read_reg(0x0001u, &rev))   return false;
    tof_write_reg(0x7FFFu, 0x02u);

    char buf[64];
    int  len = snprintf(buf, sizeof(buf),
        "  VL53L7CX dev_id=0x%02X rev_id=0x%02X\r\n", dev, rev);
    HAL_UART_Transmit(&huart4, (const uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);

    return (dev == 0xF0u && rev == 0x0Cu);
}


/* ===========================================================================
 * Error handler
 * =========================================================================*/
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(SD_MODE_GPIO_Port, SD_MODE_Pin);
        for (volatile uint32_t d = 0u; d < 300000u; ++d) { __NOP(); }
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
}
#endif
