/**
 * @file    i2c_bb.c
 * @brief   Bit-bang I2C implementation
 */

#include "i2c_bb.h"
#include "main.h"   /* for SCL_BB_*, SDA_BB_* macros and HAL */

/* ----- Tuned half-bit delay via NOP loop -----
 *
 * Cortex-M55 @ 64 MHz = 15.625 ns per cycle.
 * A single `nop` executes in 1 cycle → 15.625 ns.
 *
 * Target: ~1 us half-bit → ~64 NOPs.
 * But each bit-bang half-bit also includes:
 *   - One BSRR write  (~2 cycles)
 *   - Loop compare/branch from the surrounding code (~3 cycles)
 * So actual inline cycles needed: ~60.
 *
 * Tune HALF_BIT_NOPS up if the sensor NACKs during firmware download
 * (signal integrity or setup-time marginal), or down if you want more speed.
 *
 * -O0 is the compiler setting for Debug builds. If you switch to -O2,
 *  NOP count may need to be adjusted because surrounding overhead drops.
 */
static inline __attribute__((always_inline)) void half_bit_delay(void)
{
    __asm__ __volatile__ (
        "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
        "nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"
    );
}

/* ----- Low-level pin control ----- */
/*
 * Open-drain semantics:
 *   - "drive low" = set output 0 (actively pulls to GND)
 *   - "release high" = set output 1 (high-impedance, pull-up does the work)
 *
 * Since GPIO is configured as OUTPUT_OD in CubeMX, writing 1 to ODR puts
 * the pin into high-Z (pull-up dominates). Writing 0 pulls it low.
 */
/*
 * Direct BSRR register writes — bypasses HAL_GPIO_WritePin.
 *
 * BSRR semantics (per STM32 GPIO architecture, all families):
 *   Lower 16 bits (BSRR[15:0])  = "BS" — write 1 to set pin HIGH  (release in OD mode)
 *   Upper 16 bits (BSRR[31:16]) = "BR" — write 1 to set pin LOW   (active pull-down in OD mode)
 *   Writing 0 to any bit has no effect, so we only write the bit we want to change.
 *
 * These produce 1-2 CPU cycles each vs ~50+ cycles for HAL_GPIO_WritePin.
 *
 * IDR is the input data register. Reading a single bit of it takes ~3 cycles.
 */
static inline void SCL_LOW(void)  { SCL_BB_GPIO_Port->BSRR = (uint32_t)SCL_BB_Pin << 16; }
static inline void SCL_HIGH(void) { SCL_BB_GPIO_Port->BSRR = (uint32_t)SCL_BB_Pin;        }
static inline void SDA_LOW(void)  { SDA_BB_GPIO_Port->BSRR = (uint32_t)SDA_BB_Pin << 16; }
static inline void SDA_HIGH(void) { SDA_BB_GPIO_Port->BSRR = (uint32_t)SDA_BB_Pin;        }

static inline uint8_t SDA_READ(void) {
    return (SDA_BB_GPIO_Port->IDR & SDA_BB_Pin) ? 1u : 0u;
}

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

void i2c_bb_init(void)
{
    /* No DWT/CYCCNT setup needed — we use a tuned NOP delay now.
     * Just ensure both I2C lines are released to high (idle state). */
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
