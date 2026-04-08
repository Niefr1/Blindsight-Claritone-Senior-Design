/**
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  *
  * Claritone platform implementation — NUCLEO-N657X0-Q test target
  * I2C1 (PH9/PC1), HAL blocking mode
  *
  * Notes:
  *   - HAL_I2C_Mem_Write/Read use 16-bit register addressing, which matches
  *     the VL53L7CX's 16-bit register map.
  *   - The HAL Size parameter is uint16_t (max 65535).  The ULD internally
  *     chunks firmware writes at 0x8000 (32768), so a single HAL call is fine.
  *     A safety loop is included anyway for robustness.
  *   - I2C_TIMEOUT_MS is generous to cover the ~32 KB firmware download at
  *     400 kHz (~740 ms worst-case).
  *
  ******************************************************************************
  */

#include "platform.h"

/* ------------------------------------------------------------------------ */
/*  Tuning constants                                                        */
/* ------------------------------------------------------------------------ */

/** Maximum bytes to send in one HAL_I2C_Mem_Write call.
 *  The HAL takes uint16_t Size, but the VL53L7CX ULD never exceeds 0x8000
 *  per WrMulti call.  We cap at 0x8000 to be safe. */
#define I2C_CHUNK_SIZE   0x8000U   /* 32768 bytes */

/** HAL blocking-mode timeout (ms).  Sized for a 32 KB I2C transfer at
 *  400 kHz with margin.  Increase if you drop to 100 kHz. */
#define I2C_TIMEOUT_MS   2000U

/* ------------------------------------------------------------------------ */
/*  Single-byte helpers                                                     */
/* ------------------------------------------------------------------------ */

uint8_t VL53L7CX_RdByte(
		VL53L7CX_Platform *p_platform,
		uint16_t RegisterAdress,
		uint8_t *p_value)
{
	HAL_StatusTypeDef hal_status;

	hal_status = HAL_I2C_Mem_Read(
			p_platform->hi2c,
			p_platform->address,
			RegisterAdress,
			I2C_MEMADD_SIZE_16BIT,
			p_value,
			1,
			I2C_TIMEOUT_MS);

	return (hal_status == HAL_OK) ? 0U : 1U;
}

uint8_t VL53L7CX_WrByte(
		VL53L7CX_Platform *p_platform,
		uint16_t RegisterAdress,
		uint8_t value)
{
	HAL_StatusTypeDef hal_status;

	hal_status = HAL_I2C_Mem_Write(
			p_platform->hi2c,
			p_platform->address,
			RegisterAdress,
			I2C_MEMADD_SIZE_16BIT,
			&value,
			1,
			I2C_TIMEOUT_MS);

	return (hal_status == HAL_OK) ? 0U : 1U;
}

/* ------------------------------------------------------------------------ */
/*  Multi-byte transfers                                                    */
/* ------------------------------------------------------------------------ */

uint8_t VL53L7CX_RdMulti(
		VL53L7CX_Platform *p_platform,
		uint16_t RegisterAdress,
		uint8_t *p_values,
		uint32_t size)
{
	HAL_StatusTypeDef hal_status;
	uint32_t remaining = size;
	uint16_t chunk;
	uint16_t reg = RegisterAdress;

	while (remaining > 0U)
	{
		chunk = (remaining > I2C_CHUNK_SIZE)
				? (uint16_t)I2C_CHUNK_SIZE
				: (uint16_t)remaining;

		hal_status = HAL_I2C_Mem_Read(
				p_platform->hi2c,
				p_platform->address,
				reg,
				I2C_MEMADD_SIZE_16BIT,
				p_values,
				chunk,
				I2C_TIMEOUT_MS);

		if (hal_status != HAL_OK)
			return 1U;

		p_values  += chunk;
		reg       += chunk;
		remaining -= chunk;
	}

	return 0U;
}

uint8_t VL53L7CX_WrMulti(
		VL53L7CX_Platform *p_platform,
		uint16_t RegisterAdress,
		uint8_t *p_values,
		uint32_t size)
{
	HAL_StatusTypeDef hal_status;
	uint32_t remaining = size;
	uint16_t chunk;
	uint16_t reg = RegisterAdress;

	while (remaining > 0U)
	{
		chunk = (remaining > I2C_CHUNK_SIZE)
				? (uint16_t)I2C_CHUNK_SIZE
				: (uint16_t)remaining;

		hal_status = HAL_I2C_Mem_Write(
				p_platform->hi2c,
				p_platform->address,
				reg,
				I2C_MEMADD_SIZE_16BIT,
				p_values,
				chunk,
				I2C_TIMEOUT_MS);

		if (hal_status != HAL_OK)
			return 1U;

		p_values  += chunk;
		reg       += chunk;
		remaining -= chunk;
	}

	return 0U;
}

/* ------------------------------------------------------------------------ */
/*  Reset                                                                   */
/* ------------------------------------------------------------------------ */

uint8_t VL53L7CX_Reset_Sensor(
		VL53L7CX_Platform *p_platform)
{
	/* Pull LPn low — disables sensor I2C interface */
	HAL_GPIO_WritePin(p_platform->lpn_port, p_platform->lpn_pin, GPIO_PIN_RESET);

	/* Pulse I2C_RST low to reset the sensor's I2C state machine */
	HAL_GPIO_WritePin(p_platform->rst_port, p_platform->rst_pin, GPIO_PIN_SET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(p_platform->rst_port, p_platform->rst_pin, GPIO_PIN_RESET);

	/* Hold for 10 ms per UM3038 §4.2 */
	HAL_Delay(10);

	/* Bring sensor back up */
	HAL_GPIO_WritePin(p_platform->lpn_port, p_platform->lpn_pin, GPIO_PIN_SET);
	HAL_Delay(10);

	return 0U;
}

/* ------------------------------------------------------------------------ */
/*  Byte-swap (endianness conversion)                                       */
/* ------------------------------------------------------------------------ */

void VL53L7CX_SwapBuffer(
		uint8_t 	*buffer,
		uint16_t 	 size)
{
	/*
	 * The VL53L7CX firmware produces data in big-endian format.
	 * The Cortex-M55 (STM32N657) is little-endian.
	 * Swap every 4-byte word in place.
	 */
	uint32_t i;
	uint8_t tmp;

	for (i = 0U; i < size; i += 4U)
	{
		tmp            = buffer[i];
		buffer[i]      = buffer[i + 3U];
		buffer[i + 3U] = tmp;

		tmp            = buffer[i + 1U];
		buffer[i + 1U] = buffer[i + 2U];
		buffer[i + 2U] = tmp;
	}
}

/* ------------------------------------------------------------------------ */
/*  Delay                                                                   */
/* ------------------------------------------------------------------------ */

uint8_t VL53L7CX_WaitMs(
		VL53L7CX_Platform *p_platform,
		uint32_t TimeMs)
{
	(void)p_platform;   /* unused — HAL_Delay is global */
	HAL_Delay(TimeMs);
	return 0U;
}
