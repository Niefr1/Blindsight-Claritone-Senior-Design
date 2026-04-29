/**
 * @file    i2c_bb.c
 * @brief   Bit-bang I2C implementation
 */

#include "i2c_bb.h"
#include "main.h"   /* for SCL_BB_*, SDA_BB_* macros and HAL */

/* ----- Timing constants ----- */
/* Target: 100 kHz I2C (10 us bit period, 5 us per half-bit) */
#define I2C_BB_HALF_BIT_US      5u

/* ----- Low-level pin control ----- */
/*
 * Open-drain semantics:
 *   - "drive low" = set output 0 (actively pulls to GND)
 *   - "release high" = set output 1 (high-impedance, pull-up does the work)
 *
 * Since GPIO is configured as OUTPUT_OD in CubeMX, writing 1 to ODR puts
 * the pin into high-Z (pull-up dominates). Writing 0 pulls it low.
 */
static inline void SCL_LOW(void)  { HAL_GPIO_WritePin(SCL_BB_GPIO_Port, SCL_BB_Pin, GPIO_PIN_RESET); }
static inline void SCL_HIGH(void) { HAL_GPIO_WritePin(SCL_BB_GPIO_Port, SCL_BB_Pin, GPIO_PIN_SET);   }
static inline void SDA_LOW(void)  { HAL_GPIO_WritePin(SDA_BB_GPIO_Port, SDA_BB_Pin, GPIO_PIN_RESET); }
static inline void SDA_HIGH(void) { HAL_GPIO_WritePin(SDA_BB_GPIO_Port, SDA_BB_Pin, GPIO_PIN_SET);   }

static inline uint8_t SDA_READ(void) {
    return HAL_GPIO_ReadPin(SDA_BB_GPIO_Port, SDA_BB_Pin) == GPIO_PIN_SET ? 1u : 0u;
}

/* ----- Microsecond delay via DWT cycle counter ----- */
/*
 * DWT is part of the ARM CoreSight debug block; its CYCCNT register counts
 * CPU cycles. At HSI = 64 MHz, 1 us = 64 cycles.
 */
static void dwt_delay_us(uint32_t us) {
    uint32_t cycles = us * (SystemCoreClock / 1000000u);
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles) { /* busy wait */ }
}

static void dwt_init(void) {
    /* Enable trace block and cycle counter */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Convenience: one half-bit delay */
static inline void half_bit_delay(void) { dwt_delay_us(I2C_BB_HALF_BIT_US); }

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

void i2c_bb_init(void)
{
    dwt_init();
    /* Idle bus state: both lines released (pull-ups will bring them high) */
    SCL_HIGH();
    SDA_HIGH();
    half_bit_delay();
}

void i2c_bb_start(void)
{
    /* Ensure bus is idle: both high */
    SDA_HIGH();
    SCL_HIGH();
    half_bit_delay();

    /* START: SDA falls while SCL is high */
    SDA_LOW();
    half_bit_delay();
    SCL_LOW();
    half_bit_delay();
}

void i2c_bb_stop(void)
{
    /* Ensure SDA is low before SCL rises */
    SDA_LOW();
    half_bit_delay();
    SCL_HIGH();
    half_bit_delay();

    /* STOP: SDA rises while SCL is high */
    SDA_HIGH();
    half_bit_delay();
}

bool i2c_bb_write_byte(uint8_t data)
{
    /* Transmit 8 bits, MSB first */
    for (int i = 7; i >= 0; i--)
    {
        /* Set SDA while SCL is low */
        if ((data >> i) & 0x01) SDA_HIGH();
        else                     SDA_LOW();
        half_bit_delay();

        /* Clock out: pulse SCL high */
        SCL_HIGH();
        half_bit_delay();
        SCL_LOW();
        half_bit_delay();
    }

    /* 9th clock: read ACK from slave */
    SDA_HIGH();             /* release SDA so slave can pull it low for ACK */
    half_bit_delay();
    SCL_HIGH();
    half_bit_delay();
    uint8_t ack_bit = SDA_READ();   /* 0 = ACK, 1 = NACK */
    SCL_LOW();
    half_bit_delay();

    return (ack_bit == 0);
}

uint8_t i2c_bb_read_byte(bool send_ack)
{
    uint8_t data = 0;

    /* Release SDA so slave can drive it */
    SDA_HIGH();

    /* Receive 8 bits, MSB first */
    for (int i = 7; i >= 0; i--)
    {
        half_bit_delay();
        SCL_HIGH();
        half_bit_delay();
        data |= (SDA_READ() << i);
        SCL_LOW();
    }

    /* Send ACK (pull low) or NACK (release high) on the 9th clock */
    if (send_ack) SDA_LOW();
    else          SDA_HIGH();
    half_bit_delay();
    SCL_HIGH();
    half_bit_delay();
    SCL_LOW();
    half_bit_delay();
    SDA_HIGH();  /* release for next operation */

    return data;
}
