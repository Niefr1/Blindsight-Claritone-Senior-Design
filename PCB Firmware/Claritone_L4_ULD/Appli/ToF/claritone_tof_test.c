/**
  ******************************************************************************
  * @file    claritone_tof_test.c
  * @brief   Single-sensor VL53L7CX test on Claritone custom board
  ******************************************************************************
  */

#include "claritone_tof_test.h"
#include "platform.h"
#include "i2c_bb.h"
#include "main.h"       /* for TOF_LPN_*, TOF_I2C_RST_*, TOF_INT_* macros and huart4 */
#include <stdio.h>
#include <string.h>

/* ---- Handles ----------------------------------------------------------- */

/* huart4 is defined in main.c and used for all debug output */
extern UART_HandleTypeDef huart4;

static VL53L7CX_Configuration tof_dev;

/* ---- Debug print ------------------------------------------------------- */

static char dbg_buf[256];

static void dbg_print(const char *msg)
{
    HAL_UART_Transmit(&huart4, (const uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

#define DBG(fmt, ...) do {                                        \
    snprintf(dbg_buf, sizeof(dbg_buf), fmt, ##__VA_ARGS__);       \
    dbg_print(dbg_buf);                                           \
} while (0)

/* ---- Sensor power-on sequence ------------------------------------------ */

static void sensor_power_on(void)
{
    /* Ensure I2C_RST is LOW (normal, non-reset state) */
    HAL_GPIO_WritePin(TOF_I2C_RST_GPIO_Port, TOF_I2C_RST_Pin, GPIO_PIN_RESET);

    /* Pull LPn LOW briefly to put sensor in low-power / I2C-disabled state */
    HAL_GPIO_WritePin(TOF_LPN_GPIO_Port, TOF_LPN_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);

    /* Bring LPn HIGH — sensor I2C is now active */
    HAL_GPIO_WritePin(TOF_LPN_GPIO_Port, TOF_LPN_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
}

/* ---- Public: init ------------------------------------------------------ */

uint8_t claritone_tof_test_init(void)
{
    uint8_t status;
    uint8_t is_alive = 0;

    DBG("\r\n=== VL53L7CX Test — Claritone custom board ===\r\n");

    DBG("[INIT] Starting bit-bang I2C... ");
    i2c_bb_init();
    DBG("OK\r\n");

    DBG("[INIT] Sensor power-on sequence... ");
    sensor_power_on();
    DBG("OK\r\n");

    /* Fill platform struct — matches Felix's NUCLEO pattern */
    tof_dev.platform.address  = 0x52;   /* VL53L7CX default 8-bit I2C address */
    tof_dev.platform.lpn_port = TOF_LPN_GPIO_Port;
    tof_dev.platform.lpn_pin  = TOF_LPN_Pin;
    tof_dev.platform.rst_port = TOF_I2C_RST_GPIO_Port;
    tof_dev.platform.rst_pin  = TOF_I2C_RST_Pin;
    tof_dev.platform.int_port = TOF_INT_GPIO_Port;
    tof_dev.platform.int_pin  = TOF_INT_Pin;

    DBG("[INIT] Checking vl53l7cx_is_alive()... ");
    status = vl53l7cx_is_alive(&tof_dev, &is_alive);
    if (status != 0 || is_alive == 0) {
        DBG("FAILED (status=%u, alive=%u)\r\n", status, is_alive);
        DBG("  -> Check wiring, pull-ups, sensor address (0x52).\r\n");
        return 1;
    }
    DBG("OK - sensor detected at 0x%02X\r\n", tof_dev.platform.address);

    DBG("[INIT] vl53l7cx_init() - downloading firmware ~85 KB over bit-bang I2C...\r\n");
    DBG("       Expect ~8-10 seconds at 100 kHz.\r\n");
    uint32_t t0 = HAL_GetTick();
    status = vl53l7cx_init(&tof_dev);
    uint32_t dt = HAL_GetTick() - t0;
    if (status != 0) {
        DBG("[INIT] vl53l7cx_init() FAILED (status=%u) after %lu ms\r\n",
            status, (unsigned long)dt);
        DBG("  -> Most common cause: WrMulti I2C failure during FW download.\r\n");
        DBG("  -> Try lowering I2C speed or checking bus integrity.\r\n");
        return 1;
    }
    DBG("[INIT] vl53l7cx_init() OK - firmware loaded in %lu ms\r\n",
        (unsigned long)dt);

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

void claritone_tof_test_ranging_loop(void)
{
    uint8_t status;
    uint8_t data_ready;
    VL53L7CX_ResultsData results;

    DBG("[RANGE] Starting continuous ranging...\r\n");
    status = vl53l7cx_start_ranging(&tof_dev);
    if (status != 0) {
        DBG("[RANGE] vl53l7cx_start_ranging() FAILED (status=%u)\r\n", status);
        return;
    }

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
            HAL_Delay(5);
            continue;
        }

        status = vl53l7cx_get_ranging_data(&tof_dev, &results);
        if (status != 0) {
            DBG("[RANGE] get_ranging_data error (status=%u)\r\n", status);
            continue;
        }

        /* Print 8x8 distance map (mm). Zone status 5/6/9 = valid measurement. */
        DBG("--- frame ---\r\n");
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                int zone = row * 8 + col;
                int16_t dist = results.distance_mm[zone];
                uint8_t tgt_status = results.target_status[zone];

                if (tgt_status == 5 || tgt_status == 6 || tgt_status == 9)
                    DBG("%5d", dist);
                else
                    DBG("    -");
            }
            DBG("\r\n");
        }
        DBG("\r\n");

        HAL_GPIO_TogglePin(SCROLL_1_GPIO_Port, SCROLL_1_Pin);  /* heartbeat */
    }
}