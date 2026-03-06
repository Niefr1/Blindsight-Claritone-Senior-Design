#ifndef OV9281_H
#define OV9281_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define OV9281_OK                 0
#define OV9281_ERROR             -1

/* SCCB / I2C 7-bit address */
#define OV9281_I2C_ADDR          (0x60 >> 1)   /* adjust if needed */

/* Common registers */
#define OV9281_REG_CHIP_ID_H     0x300A
#define OV9281_REG_CHIP_ID_L     0x300B
#define OV9281_CHIP_ID           0x9281

#define OV9281_REG_MODE_SELECT   0x0100
#define OV9281_MODE_STANDBY      0x00
#define OV9281_MODE_STREAMING    0x01

#define OV9281_REG_SOFT_RESET    0x0103
#define OV9281_SOFT_RESET        0x01

typedef struct
{
    uint16_t Reg;
    uint8_t  Val;
} OV9281_RegVal_t;

typedef struct
{
    uint16_t Width;
    uint16_t Height;
    uint8_t  Fps;
} OV9281_Mode_t;

/* BSP hooks you must implement for your board */
int32_t OV9281_IO_Init(void);
int32_t OV9281_IO_DeInit(void);
int32_t OV9281_IO_WriteReg(uint16_t reg, uint8_t value);
int32_t OV9281_IO_ReadReg(uint16_t reg, uint8_t *value);
void    OV9281_DelayMs(uint32_t ms);
void    OV9281_ResetPin(uint8_t state);
void    OV9281_PowerDownPin(uint8_t state);

/* Sensor API */
int32_t OV9281_Init(const OV9281_Mode_t *mode);
int32_t OV9281_ReadID(uint32_t *id);
int32_t OV9281_Start(void);
int32_t OV9281_Stop(void);

#ifdef __cplusplus
}
#endif

#endif