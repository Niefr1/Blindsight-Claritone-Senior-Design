/**
  ******************************************************************************
  * @file    nucleo_tof_test.h
  * @brief   Single-sensor VL53L7CX test on NUCLEO-N657X0-Q
  *
  * Wiring (SATEL-VL53L7CX → NUCLEO morpho/Arduino headers):
  *
  *   SATEL pin 1  (GND)      →  GND
  *   SATEL pin 2  (IOVDD)    →  3V3
  *   SATEL pin 3  (AVDD)     →  3V3    (SATEL-VL53L7CX accepts 2.8–3.5V)
  *   SATEL pin 5  (LPn)      →  PA3    (Arduino D10, CN14-3)
  *   SATEL pin 6  (SCL)      →  PH9    (Morpho CN15-3, I2C1_SCL)
  *   SATEL pin 7  (SDA)      →  PC1    (Morpho CN15-5, I2C1_SDA)
  *   SATEL pin 8  (I2C_RST)  →  PD7    (Arduino D9, CN14-2)
  *   SATEL pin 9  (INT)      →  PD12   (Arduino D8, CN13-1) [optional]
  *
  * I2C1 pull-ups (R88 = 1.5kΩ, R89 = 1.5kΩ) are already populated
  * on the NUCLEO-N657X0-Q for PH9/PC1.
  *
  * Debug output: USART3 TX on PD8 (Arduino D1), 115200-8-N-1.
  *               Connect a USB-UART adapter or use the STLINK VCP if
  *               routed to USART3.
  *
  ******************************************************************************
  */

#ifndef NUCLEO_TOF_TEST_H
#define NUCLEO_TOF_TEST_H

#include "stm32n6xx_hal.h"
#include "vl53l7cx_api.h"

/* ---- Pin assignments --------------------------------------------------- */

/*
 * AF numbers verified against DS14791 Table 19 (AF0–AF7):
 *   PH9 AF4 = I2C1_SCL
 *   PC1 AF4 = I2C1_SDA
 *   PD8 AF7 = USART3_TX
 *   PD9 AF7 = USART3_RX
 *
 * If your HAL version doesn't define GPIO_AF4_I2C1, use the numeric
 * literal directly.  The AF number is what matters, not the macro name.
 */
#ifndef GPIO_AF4_I2C1
#define GPIO_AF4_I2C1   ((uint8_t)0x04)
#endif
#ifndef GPIO_AF7_USART3
#define GPIO_AF7_USART3 ((uint8_t)0x07)
#endif

/* I2C1 */
#define TOF_I2C_SCL_PORT     GPIOH
#define TOF_I2C_SCL_PIN      GPIO_PIN_9
#define TOF_I2C_SCL_AF       GPIO_AF4_I2C1

#define TOF_I2C_SDA_PORT     GPIOC
#define TOF_I2C_SDA_PIN      GPIO_PIN_1
#define TOF_I2C_SDA_AF       GPIO_AF4_I2C1

/* Sensor control GPIOs */
#define TOF_LPN_PORT         GPIOA
#define TOF_LPN_PIN          GPIO_PIN_3

#define TOF_RST_PORT         GPIOD
#define TOF_RST_PIN          GPIO_PIN_7

#define TOF_INT_PORT         GPIOD
#define TOF_INT_PIN          GPIO_PIN_12

/* Debug UART (USART3 on Arduino D0/D1) */
#define DBG_UART_TX_PORT     GPIOD
#define DBG_UART_TX_PIN      GPIO_PIN_8
#define DBG_UART_TX_AF       GPIO_AF7_USART3

#define DBG_UART_RX_PORT     GPIOD
#define DBG_UART_RX_PIN      GPIO_PIN_9
#define DBG_UART_RX_AF       GPIO_AF7_USART3

/* ---- Public API -------------------------------------------------------- */

/**
 * @brief  Initialise I2C1, GPIOs, USART3 debug, and the VL53L7CX sensor.
 *         Prints progress to USART3 at each step.
 * @retval 0 on success, non-zero on failure (check USART3 output).
 */
uint8_t tof_test_init(void);

/**
 * @brief  Run a continuous ranging loop — prints 8x8 distance data to
 *         USART3 every time new data is ready.  Does not return.
 */
void tof_test_ranging_loop(void);

#endif /* NUCLEO_TOF_TEST_H */
