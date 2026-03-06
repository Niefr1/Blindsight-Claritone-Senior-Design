#include "ov9281.h"

/*
 * These tables are placeholders.
 * Replace them with real OV9281 init values from a working driver/datasheet sequence.
 */

static const OV9281_RegVal_t ov9281_global_init[] =
{
    {0x0103, 0x01}, /* software reset */
    /* add real init regs here */
    {0x0000, 0x00}  /* end marker */
};

static const OV9281_RegVal_t ov9281_mode_640x400_raw8[] =
{
    /* replace with real mode register sequence */
    {0x0100, 0x00}, /* standby before reconfig */
    /* timing / crop / format regs here */
    {0x0000, 0x00}
};

static int32_t OV9281_WriteTable(const OV9281_RegVal_t *tbl)
{
    uint32_t i = 0;

    while (!(tbl[i].Reg == 0x0000 && tbl[i].Val == 0x00))
    {
        if (OV9281_IO_WriteReg(tbl[i].Reg, tbl[i].Val) != OV9281_OK)
        {
            return OV9281_ERROR;
        }
        i++;
    }
    return OV9281_OK;
}

int32_t OV9281_ReadID(uint32_t *id)
{
    uint8_t idh = 0, idl = 0;

    if (id == 0)
        return OV9281_ERROR;

    if (OV9281_IO_ReadReg(OV9281_REG_CHIP_ID_H, &idh) != OV9281_OK)
        return OV9281_ERROR;

    if (OV9281_IO_ReadReg(OV9281_REG_CHIP_ID_L, &idl) != OV9281_OK)
        return OV9281_ERROR;

    *id = ((uint32_t)idh << 8) | idl;
    return OV9281_OK;
}

int32_t OV9281_Init(const OV9281_Mode_t *mode)
{
    uint32_t id = 0;

    if (mode == 0)
        return OV9281_ERROR;

    if (OV9281_IO_Init() != OV9281_OK)
        return OV9281_ERROR;

    /* Power/reset sequence */
    OV9281_PowerDownPin(1);
    OV9281_ResetPin(0);
    OV9281_DelayMs(2);

    OV9281_PowerDownPin(0);
    OV9281_DelayMs(2);

    OV9281_ResetPin(1);
    OV9281_DelayMs(10);

    if (OV9281_ReadID(&id) != OV9281_OK)
        return OV9281_ERROR;

    if (id != OV9281_CHIP_ID)
        return OV9281_ERROR;

    if (OV9281_WriteTable(ov9281_global_init) != OV9281_OK)
        return OV9281_ERROR;

    OV9281_DelayMs(5);

    if ((mode->Width == 640) && (mode->Height == 400))
    {
        if (OV9281_WriteTable(ov9281_mode_640x400_raw8) != OV9281_OK)
            return OV9281_ERROR;
    }
    else
    {
        return OV9281_ERROR;
    }

    return OV9281_OK;
}

int32_t OV9281_Start(void)
{
    return OV9281_IO_WriteReg(OV9281_REG_MODE_SELECT, OV9281_MODE_STREAMING);
}

int32_t OV9281_Stop(void)
{
    return OV9281_IO_WriteReg(OV9281_REG_MODE_SELECT, OV9281_MODE_STANDBY);
}