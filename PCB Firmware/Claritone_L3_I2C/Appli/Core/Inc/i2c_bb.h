/**
 * @file    i2c_bb.h
 * @brief   Bit-bang I2C driver for Claritone ToF bus (PB8=SCL, PB9=SDA)
 *
 * STM32N657 does not expose an I2C alternate function on PB8/PB9
 * (verified against DS14791 Table 19), so the ToF I2C bus is implemented
 * in software. Target speed: 100 kHz (standard mode) for bring-up margin.
 *
 * Both pins configured as GPIO_OUTPUT_OD (open-drain) with external
 * pull-ups R21 (SCL) and R22 (SDA) to 3.3V.
 */

#ifndef I2C_BB_H
#define I2C_BB_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize bit-bang I2C (sets idle state: SCL=HIGH, SDA=HIGH)
 *        Also initializes DWT cycle counter for microsecond timing.
 *        Must be called once before any other i2c_bb_* function.
 */
void i2c_bb_init(void);

/**
 * @brief Send I2C START condition (SDA falls while SCL high)
 */
void i2c_bb_start(void);

/**
 * @brief Send I2C STOP condition (SDA rises while SCL high)
 */
void i2c_bb_stop(void);

/**
 * @brief Write one byte to the bus and read the ACK
 * @param data  Byte to transmit
 * @return true if slave ACKed (pulled SDA low on 9th bit), false if NACK
 */
bool i2c_bb_write_byte(uint8_t data);

/**
 * @brief Read one byte from the bus and send ACK or NACK
 * @param send_ack  true = send ACK (expecting more bytes), false = send NACK (last byte)
 * @return          The byte received
 */
uint8_t i2c_bb_read_byte(bool send_ack);

#endif /* I2C_BB_H */
