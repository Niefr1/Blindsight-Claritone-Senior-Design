/**
  ******************************************************************************
  * @file    platform.c
  * @brief   VL53L7CX platform implementation for Claritone custom board
  *
  * Routes ST's ULD API to the bit-bang I2C driver (see i2c_bb.c).
  *
  * VL53L7CX I2C protocol (per DS13865 Figures 12-14):
  *   Write N bytes to reg R: S | 0x52 | R[15:8] | R[7:0] | D0 | D1 | ... | P
  *   Read  N bytes from reg R:
  *     S | 0x52 | R[15:8] | R[7:0] | Sr | 0x53 | D0 (ack) | ... | Dn-1 (nack) | P
  *
  * Register auto-increment is used for multi-byte transfers.
  *
  * Copyright (c) 2021 STMicroelectronics. All rights reserved.
  * Modifications for Claritone custom board by Joseph Cabrera.
  ******************************************************************************
  */

#include "platform.h"
#include "i2c_bb.h"

uint8_t VL53L7CX_RdByte(
        VL53L7CX_Platform *p_platform,
        uint16_t RegisterAdress,
        uint8_t *p_value)
{
    uint8_t addr_w = (uint8_t)p_platform->address;
    uint8_t addr_r = (uint8_t)(p_platform->address | 1U);

    i2c_bb_start();
    if (!i2c_bb_write_byte(addr_w))                    goto fail;
    if (!i2c_bb_write_byte((uint8_t)(RegisterAdress >> 8))) goto fail;
    if (!i2c_bb_write_byte((uint8_t)(RegisterAdress)))      goto fail;

    i2c_bb_start();
    if (!i2c_bb_write_byte(addr_r))                    goto fail;
    *p_value = i2c_bb_read_byte(false);
    i2c_bb_stop();
    return 0U;

fail:
    i2c_bb_stop();
    return 1U;
}

uint8_t VL53L7CX_WrByte(
        VL53L7CX_Platform *p_platform,
        uint16_t RegisterAdress,
        uint8_t value)
{
    uint8_t addr_w = (uint8_t)p_platform->address;

    i2c_bb_start();
    if (!i2c_bb_write_byte(addr_w))                    goto fail;
    if (!i2c_bb_write_byte((uint8_t)(RegisterAdress >> 8))) goto fail;
    if (!i2c_bb_write_byte((uint8_t)(RegisterAdress)))      goto fail;
    if (!i2c_bb_write_byte(value))                     goto fail;
    i2c_bb_stop();
    return 0U;

fail:
    i2c_bb_stop();
    return 1U;
}

uint8_t VL53L7CX_RdMulti(
        VL53L7CX_Platform *p_platform,
        uint16_t RegisterAdress,
        uint8_t *p_values,
        uint32_t size)
{
    uint8_t addr_w = (uint8_t)p_platform->address;
    uint8_t addr_r = (uint8_t)(p_platform->address | 1U);

    if (size == 0U) return 0U;

    i2c_bb_start();
    if (!i2c_bb_write_byte(addr_w))                    goto fail;
    if (!i2c_bb_write_byte((uint8_t)(RegisterAdress >> 8))) goto fail;
    if (!i2c_bb_write_byte((uint8_t)(RegisterAdress)))      goto fail;

    i2c_bb_start();
    if (!i2c_bb_write_byte(addr_r))                    goto fail;

    for (uint32_t i = 0U; i < size - 1U; i++) {
        p_values[i] = i2c_bb_read_byte(true);
    }
    p_values[size - 1U] = i2c_bb_read_byte(false);

    i2c_bb_stop();
    return 0U;

fail:
    i2c_bb_stop();
    return 1U;
}

uint8_t VL53L7CX_WrMulti(
        VL53L7CX_Platform *p_platform,
        uint16_t RegisterAdress,
        uint8_t *p_values,
        uint32_t size)
{
    uint8_t addr_w = (uint8_t)p_platform->address;

    if (size == 0U) return 0U;

    i2c_bb_start();
    if (!i2c_bb_write_byte(addr_w))                    goto fail;
    if (!i2c_bb_write_byte((uint8_t)(RegisterAdress >> 8))) goto fail;
    if (!i2c_bb_write_byte((uint8_t)(RegisterAdress)))      goto fail;

    for (uint32_t i = 0U; i < size; i++) {
        if (!i2c_bb_write_byte(p_values[i]))           goto fail;
    }

    i2c_bb_stop();
    return 0U;

fail:
    i2c_bb_stop();
    return 1U;
}

uint8_t VL53L7CX_Reset_Sensor(
        VL53L7CX_Platform *p_platform)
{
    HAL_GPIO_WritePin(p_platform->lpn_port, p_platform->lpn_pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(p_platform->rst_port, p_platform->rst_pin, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(p_platform->rst_port, p_platform->rst_pin, GPIO_PIN_RESET);
    HAL_Delay(10);

    HAL_GPIO_WritePin(p_platform->lpn_port, p_platform->lpn_pin, GPIO_PIN_SET);
    HAL_Delay(10);

    return 0U;
}

void VL53L7CX_SwapBuffer(
        uint8_t  *buffer,
        uint16_t  size)
{
    uint32_t i;
    uint8_t  tmp;

    for (i = 0U; i < size; i += 4U) {
        tmp            = buffer[i];
        buffer[i]      = buffer[i + 3U];
        buffer[i + 3U] = tmp;

        tmp            = buffer[i + 1U];
        buffer[i + 1U] = buffer[i + 2U];
        buffer[i + 2U] = tmp;
    }
}

uint8_t VL53L7CX_WaitMs(
        VL53L7CX_Platform *p_platform,
        uint32_t TimeMs)
{
    (void)p_platform;
    HAL_Delay(TimeMs);
    return 0U;
}