/**
  ******************************************************************************
  * @file    claritone_tof_test.h
  * @brief   Single-sensor VL53L7CX test on Claritone custom board
  *
  * On-board VL53L7CX wiring (per schematic v246):
  *   SCL       ← PB8   (TOF_SCL, bit-bang)
  *   SDA       ↔ PB9   (TOF_SDA, bit-bang)
  *   LPn       ← PD1   (TOF_LPN, MCU output, active-high enable)
  *   I2C_RST   ← PE1   (TOF_I2C_RST, MCU output, active-high reset)
  *   INT       → PE0   (TOF_INT, MCU input, open-drain from sensor)
  *
  * External pull-ups: R21 (SCL, 3.3V), R22 (SDA, 3.3V), R6 (INT, 3.3V).
  *
  * Debug output: UART4 via huart4, 115200-8-N-1, to STLINK VCP.
  ******************************************************************************
  */

#ifndef CLARITONE_TOF_TEST_H
#define CLARITONE_TOF_TEST_H

#include <stdint.h>
#include "vl53l7cx_api.h"

/**
 * @brief  Initialize the bit-bang I2C, power up the VL53L7CX, run
 *         vl53l7cx_is_alive() + vl53l7cx_init() (firmware download),
 *         and configure 8x8 resolution at 15 Hz.
 *         Prints progress on UART4.
 * @retval 0 on success, non-zero on failure.
 */
uint8_t claritone_tof_test_init(void);

/**
 * @brief  Continuous ranging loop — prints 8x8 distance map (mm)
 *         on UART4 every time new data is ready. Does not return.
 */
void claritone_tof_test_ranging_loop(void);

#endif /* CLARITONE_TOF_TEST_H */