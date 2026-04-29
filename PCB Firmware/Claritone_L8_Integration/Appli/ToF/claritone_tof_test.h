/**
  ******************************************************************************
  * @file    claritone_tof_test.h
  * @brief   Single-sensor VL53L7CX test on Claritone custom board
  *
  * On-board VL53L7CX wiring (per schematic v246):
  *   SCL       <- PB8   (TOF_SCL, bit-bang)
  *   SDA       <> PB9   (TOF_SDA, bit-bang)
  *   LPn       <- PD1   (TOF_LPN, MCU output, active-high enable)
  *   I2C_RST   <- PE1   (TOF_I2C_RST, MCU output, active-high reset)
  *   INT       -> PE0   (TOF_INT, MCU input, open-drain from sensor)
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
 *         configure 8x8 resolution at 15 Hz, and start ranging.
 *         Prints progress on UART4.
 * @retval 0 on success, non-zero on failure.
 */
uint8_t claritone_tof_test_init(void);

/**
 * @brief  Continuous ranging loop — prints 8x8 distance map (mm)
 *         on UART4 every time new data is ready. Does not return.
 *         NOTE: Not used in combined firmware — use claritone_tof_poll()
 *         instead for non-blocking operation.
 */
void claritone_tof_test_ranging_loop(void);

/**
 * @brief  Non-blocking single poll of the sensor.
 *         Call periodically from the main loop after claritone_tof_test_init().
 * @retval  0  fresh result stored — call claritone_tof_last_mm() to read it
 *          1  data not ready yet — try again next cycle
 *         -1  vl53l7cx_check_data_ready() error
 *         -2  vl53l7cx_get_ranging_data() error
 */
int claritone_tof_poll(void);

/**
 * @brief  Returns the closest valid zone distance from the last
 *         successful claritone_tof_poll() call.
 * @retval Distance in mm, or 0 if no valid result yet.
 */
uint32_t claritone_tof_last_mm(void);

#endif /* CLARITONE_TOF_TEST_H */
