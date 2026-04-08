/**
  ******************************************************************************
  * @file    nucleo_tof_test.c
  * @brief   Single-sensor VL53L7CX test on NUCLEO-N657X0-Q
  ******************************************************************************
  */

#include "nucleo_tof_test.h"
#include "platform.h"
#include <stdio.h>
#include <string.h>

/* ---- Handles ----------------------------------------------------------- */

static I2C_HandleTypeDef        hi2c1;
static UART_HandleTypeDef       huart3;
static VL53L7CX_Configuration   tof_dev;

/* ---- Debug print ------------------------------------------------------- */

static char dbg_buf[256];

static void dbg_print(const char *msg)
{
	HAL_UART_Transmit(&huart3, (uint8_t *)msg, strlen(msg), 100);
}

/* printf-style debug helper */
#define DBG(fmt, ...) do { \
	snprintf(dbg_buf, sizeof(dbg_buf), fmt, ##__VA_ARGS__); \
	dbg_print(dbg_buf); \
} while(0)

/* ---- GPIO init --------------------------------------------------------- */

static void init_gpio(void)
{
	GPIO_InitTypeDef gpio = {0};

	/* Enable clocks for all GPIO ports we use */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();

	/* LPn — output push-pull, default HIGH (sensor enabled) */
	gpio.Pin   = TOF_LPN_PIN;
	gpio.Mode  = GPIO_MODE_OUTPUT_PP;
	gpio.Pull  = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(TOF_LPN_PORT, &gpio);
	HAL_GPIO_WritePin(TOF_LPN_PORT, TOF_LPN_PIN, GPIO_PIN_SET);

	/* I2C_RST — output push-pull, default LOW (not in reset) */
	gpio.Pin = TOF_RST_PIN;
	HAL_GPIO_Init(TOF_RST_PORT, &gpio);
	HAL_GPIO_WritePin(TOF_RST_PORT, TOF_RST_PIN, GPIO_PIN_RESET);

	/* INT — input (optional, for interrupt-driven ranging) */
	gpio.Pin  = TOF_INT_PIN;
	gpio.Mode = GPIO_MODE_INPUT;
	gpio.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(TOF_INT_PORT, &gpio);
}

/* ---- I2C1 init --------------------------------------------------------- */

static uint8_t init_i2c1(void)
{
	GPIO_InitTypeDef gpio = {0};

	__HAL_RCC_I2C1_CLK_ENABLE();

	/* PH9 = I2C1_SCL (AF4), PC1 = I2C1_SDA (AF4) */
	/* Open-drain, pull-ups are external (R88/R89 on NUCLEO) */
	gpio.Mode      = GPIO_MODE_AF_OD;
	gpio.Pull      = GPIO_NOPULL;
	gpio.Speed     = GPIO_SPEED_FREQ_HIGH;

	gpio.Pin       = TOF_I2C_SCL_PIN;
	gpio.Alternate = TOF_I2C_SCL_AF;
	HAL_GPIO_Init(TOF_I2C_SCL_PORT, &gpio);

	gpio.Pin       = TOF_I2C_SDA_PIN;
	gpio.Alternate = TOF_I2C_SDA_AF;
	HAL_GPIO_Init(TOF_I2C_SDA_PORT, &gpio);

	/* I2C1 configuration — 400 kHz Fast Mode */
	hi2c1.Instance              = I2C1;
	hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
	hi2c1.Init.OwnAddress1      = 0x00;
	hi2c1.Init.OwnAddress2      = 0x00;

	/*
	 * TODO: Verify this timing value against your actual I2C kernel clock.
	 *
	 * Use STM32CubeMX → I2C1 → Parameter Settings → Timing to compute
	 * the correct value for your clock configuration.
	 *
	 * The value below is a PLACEHOLDER for 400 kHz Fm with a 64 MHz
	 * I2C kernel clock (typical HSI).  If your kernel clock is different,
	 * this WILL NOT produce 400 kHz and may not work at all.
	 *
	 * To find your I2C kernel clock: check RCC_CCIPR1/CCIPR4 registers
	 * or the CubeMX Clock Configuration tab → I2C1 clock mux.
	 */
	hi2c1.Init.Timing = 0x00B03FDB;  /* PLACEHOLDER — see note above */

	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
		return 1;

	/* Enable the analog noise filter */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
		return 1;

	return 0;
}

/* ---- USART3 debug init ------------------------------------------------- */

static uint8_t init_uart_debug(void)
{
	GPIO_InitTypeDef gpio = {0};

	__HAL_RCC_USART3_CLK_ENABLE();

	/* PD8 = USART3_TX (AF7) */
	gpio.Pin       = DBG_UART_TX_PIN;
	gpio.Mode      = GPIO_MODE_AF_PP;
	gpio.Pull      = GPIO_NOPULL;
	gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
	gpio.Alternate = DBG_UART_TX_AF;
	HAL_GPIO_Init(DBG_UART_TX_PORT, &gpio);

	/* PD9 = USART3_RX (AF7) — optional for TX-only debug */
	gpio.Pin       = DBG_UART_RX_PIN;
	gpio.Alternate = DBG_UART_RX_AF;
	HAL_GPIO_Init(DBG_UART_RX_PORT, &gpio);

	huart3.Instance          = USART3;
	huart3.Init.BaudRate     = 115200;
	huart3.Init.WordLength   = UART_WORDLENGTH_8B;
	huart3.Init.StopBits     = UART_STOPBITS_1;
	huart3.Init.Parity       = UART_PARITY_NONE;
	huart3.Init.Mode         = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;

	if (HAL_UART_Init(&huart3) != HAL_OK)
		return 1;

	return 0;
}

/* ---- Sensor power-on sequence ------------------------------------------ */

static void sensor_power_on(void)
{
	/* Ensure I2C_RST is LOW (not in reset) */
	HAL_GPIO_WritePin(TOF_RST_PORT, TOF_RST_PIN, GPIO_PIN_RESET);

	/* Pull LPn LOW briefly to reset sensor I2C state */
	HAL_GPIO_WritePin(TOF_LPN_PORT, TOF_LPN_PIN, GPIO_PIN_RESET);
	HAL_Delay(10);

	/* Bring LPn HIGH — sensor I2C is now active */
	HAL_GPIO_WritePin(TOF_LPN_PORT, TOF_LPN_PIN, GPIO_PIN_SET);
	HAL_Delay(10);
}

/* ---- Public: init ------------------------------------------------------ */

uint8_t tof_test_init(void)
{
	uint8_t status;
	uint8_t is_alive = 0;

	/* 1. Peripherals */
	init_gpio();

	if (init_uart_debug() != 0)
		return 1;   /* can't even print errors */

	DBG("\r\n=== VL53L7CX Test — NUCLEO-N657X0-Q ===\r\n");

	DBG("[INIT] Configuring I2C1 (PH9/PC1)... ");
	if (init_i2c1() != 0) {
		DBG("FAILED\r\n");
		return 1;
	}
	DBG("OK\r\n");

	/* 2. Sensor power-on */
	DBG("[INIT] Sensor power-on sequence... ");
	sensor_power_on();
	DBG("OK\r\n");

	/* 3. Fill platform struct */
	tof_dev.platform.address = 0x52;   /* VL53L7CX default 8-bit address */
	tof_dev.platform.hi2c    = &hi2c1;
	tof_dev.platform.lpn_port = TOF_LPN_PORT;
	tof_dev.platform.lpn_pin  = TOF_LPN_PIN;
	tof_dev.platform.rst_port = TOF_RST_PORT;
	tof_dev.platform.rst_pin  = TOF_RST_PIN;
	tof_dev.platform.int_port = TOF_INT_PORT;
	tof_dev.platform.int_pin  = TOF_INT_PIN;

	/* 4. Check sensor is alive */
	DBG("[INIT] Checking vl53l7cx_is_alive()... ");
	status = vl53l7cx_is_alive(&tof_dev, &is_alive);
	if (status != 0 || is_alive == 0) {
		DBG("FAILED (status=%u, alive=%u)\r\n", status, is_alive);
		DBG("  → Check wiring: SDA, SCL, IOVDD, AVDD, LPn\r\n");
		DBG("  → Check pull-ups on I2C bus\r\n");
		DBG("  → Check sensor address (default 0x52)\r\n");
		return 1;
	}
	DBG("OK — sensor detected at 0x%02X\r\n", tof_dev.platform.address);

	/* 5. Initialize sensor (downloads ~84KB firmware over I2C) */
	DBG("[INIT] Running vl53l7cx_init() — downloading firmware...\r\n");
	DBG("       This takes several seconds at 400 kHz.\r\n");
	status = vl53l7cx_init(&tof_dev);
	if (status != 0) {
		DBG("[INIT] vl53l7cx_init() FAILED (status=%u)\r\n", status);
		DBG("  → Most common cause: WrMulti I2C failure during FW download\r\n");
		DBG("  → Check I2C timing config and bus integrity\r\n");
		return 1;
	}
	DBG("[INIT] vl53l7cx_init() OK — firmware loaded\r\n");

	/* 6. Configure: 8x8 resolution, 15 Hz */
	DBG("[INIT] Setting 8x8 resolution... ");
	status = vl53l7cx_set_resolution(&tof_dev, VL53L7CX_RESOLUTION_8X8);
	if (status != 0) {
		DBG("FAILED (status=%u)\r\n", status);
		return 1;
	}
	DBG("OK\r\n");

	DBG("[INIT] Setting 15 Hz ranging frequency... ");
	status = vl53l7cx_set_ranging_frequency_hz(&tof_dev, 15);
	if (status != 0) {
		DBG("FAILED (status=%u)\r\n", status);
		return 1;
	}
	DBG("OK\r\n");

	DBG("[INIT] Sensor ready.\r\n\r\n");
	return 0;
}

/* ---- Public: ranging loop ---------------------------------------------- */

void tof_test_ranging_loop(void)
{
	uint8_t status;
	uint8_t data_ready;
	VL53L7CX_ResultsData results;

	/* Start ranging */
	DBG("[RANGE] Starting continuous ranging...\r\n");
	status = vl53l7cx_start_ranging(&tof_dev);
	if (status != 0) {
		DBG("[RANGE] vl53l7cx_start_ranging() FAILED (status=%u)\r\n", status);
		return;
	}

	/* Poll forever */
	while (1)
	{
		data_ready = 0;
		status = vl53l7cx_check_data_ready(&tof_dev, &data_ready);

		if (status != 0) {
			DBG("[RANGE] check_data_ready error (status=%u)\r\n", status);
			HAL_Delay(10);
			continue;
		}

		if (!data_ready) {
			HAL_Delay(5);  /* avoid hammering the bus */
			continue;
		}

		/* Read ranging data */
		status = vl53l7cx_get_ranging_data(&tof_dev, &results);
		if (status != 0) {
			DBG("[RANGE] get_ranging_data error (status=%u)\r\n", status);
			continue;
		}

		/* Print 8x8 distance map (mm) */
		/* Zone layout: row-major, zone 0 = top-right of actual scene
		 * (due to lens flip — see UM3038 §2.2) */
		DBG("--- frame ---\r\n");
		for (int row = 0; row < 8; row++)
		{
			for (int col = 0; col < 8; col++)
			{
				int zone = row * 8 + col;
				int16_t dist = results.distance_mm[zone];
				uint8_t  tgt_status = results.target_status[zone];

				/* Only print valid targets (status 5, 6, or 9) */
				if (tgt_status == 5 || tgt_status == 6 || tgt_status == 9)
					DBG("%5d", dist);
				else
					DBG("    -");
			}
			DBG("\r\n");
		}
		DBG("\r\n");
	}
}
