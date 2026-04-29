/**
  ******************************************************************************
  * @file    platform.h
  * @brief   VL53L7CX platform abstraction for Claritone custom board
  *
  * Target: STM32N657X0H3Q custom PCB, on-board VL53L7CX ToF sensor
  *
  * I2C transport: software bit-bang on PB8 (SCL) / PB9 (SDA).
  * The STM32N657 has no I2C alternate function on PB8/PB9
  * (verified against DS14791 Table 19), so a bit-bang driver
  * (see i2c_bb.h) replaces the hardware I2C peripheral here.
  *
  * Sensor control lines:
  *   LPn      → PD1  (TOF_LPN)
  *   I2C_RST  → PE1  (TOF_I2C_RST)
  *   INT      → PE0  (TOF_INT)
  *
  * Derived from the STMicroelectronics VL53L7CX ULD reference
  * platform layer. Original copyright notice preserved below.
  *
  * Copyright (c) 2021 STMicroelectronics. All rights reserved.
  * This software is licensed under terms that can be found in the LICENSE
  * file in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_
#pragma once

#include <stdint.h>
#include <string.h>

#include "stm32n6xx_hal.h"

typedef struct
{
    uint16_t         address;

    GPIO_TypeDef    *lpn_port;
    uint16_t         lpn_pin;

    GPIO_TypeDef    *int_port;
    uint16_t         int_pin;

    GPIO_TypeDef    *rst_port;
    uint16_t         rst_pin;

} VL53L7CX_Platform;

#define VL53L7CX_NB_TARGET_PER_ZONE     1U

// #define VL53L7CX_USE_RAW_FORMAT

// #define VL53L7CX_DISABLE_AMBIENT_PER_SPAD
// #define VL53L7CX_DISABLE_NB_SPADS_ENABLED
// #define VL53L7CX_DISABLE_NB_TARGET_DETECTED
// #define VL53L7CX_DISABLE_SIGNAL_PER_SPAD
// #define VL53L7CX_DISABLE_RANGE_SIGMA_MM
// #define VL53L7CX_DISABLE_DISTANCE_MM
// #define VL53L7CX_DISABLE_REFLECTANCE_PERCENT
// #define VL53L7CX_DISABLE_TARGET_STATUS
// #define VL53L7CX_DISABLE_MOTION_INDICATOR

uint8_t VL53L7CX_RdByte(
        VL53L7CX_Platform *p_platform,
        uint16_t RegisterAdress,
        uint8_t *p_value);

uint8_t VL53L7CX_WrByte(
        VL53L7CX_Platform *p_platform,
        uint16_t RegisterAdress,
        uint8_t value);

uint8_t VL53L7CX_RdMulti(
        VL53L7CX_Platform *p_platform,
        uint16_t RegisterAdress,
        uint8_t *p_values,
        uint32_t size);

uint8_t VL53L7CX_WrMulti(
        VL53L7CX_Platform *p_platform,
        uint16_t RegisterAdress,
        uint8_t *p_values,
        uint32_t size);

uint8_t VL53L7CX_Reset_Sensor(
        VL53L7CX_Platform *p_platform);

void VL53L7CX_SwapBuffer(
        uint8_t  *buffer,
        uint16_t  size);

uint8_t VL53L7CX_WaitMs(
        VL53L7CX_Platform *p_platform,
        uint32_t TimeMs);

#endif  /* _PLATFORM_H_ */