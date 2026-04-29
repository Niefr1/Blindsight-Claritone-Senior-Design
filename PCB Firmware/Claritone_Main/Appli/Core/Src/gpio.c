/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   GPIO initialisation for Claritone_Main
  *
  * Pin summary
  * -----------
  *  Outputs (push-pull unless noted):
  *    PB1  SD_MODE      — MAX98357A amp enable (HIGH = on)
  *    PB8  SCL_BB       — I2C bit-bang clock (open-drain)
  *    PB9  SDA_BB       — I2C bit-bang data  (open-drain)
  *    PD1  TOF_LPN      — VL53L7CX LP_n pin
  *    PE0  TOF_INT      — VL53L7CX INT output (MCU drives low to clear)
  *    PE1  TOF_I2C_RST  — VL53L7CX I2C reset
  *
  *  Inputs (pull-up, active-low):
  *    PC4  BTN_TONE     — Tone-preset pushbutton
  *    PC6  BTN_SENS     — Sensitivity pushbutton
  *    PD8  SCROLL_1     — Encoder CLK / channel A
  *    PD9  SCROLL_NO    — Encoder push-switch (reserved)
  *    PA9  SCROLL_2     — Encoder DT  / channel B
  *
  *  Note: UART4 (PA0/PA1) and SAI1 (PB0/PB2/PB6) are configured by their
  *        respective CubeMX-generated peripheral init files (usart.c / sai.c).
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"
#include "main.h"

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                              */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */
/* USER CODE END 1 */

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* -------------------------------------------------------------------- */
    /* Enable GPIO port clocks                                               */
    /* -------------------------------------------------------------------- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* -------------------------------------------------------------------- */
    /* Set initial output levels BEFORE configuring pin directions          */
    /* -------------------------------------------------------------------- */
    /* Amp enable: amps on at boot */
    HAL_GPIO_WritePin(SD_MODE_GPIO_Port, SD_MODE_Pin, GPIO_PIN_SET);

    /* I2C bit-bang: both lines idle-high (open-drain; pull-ups do the work) */
    HAL_GPIO_WritePin(SCL_BB_GPIO_Port, SCL_BB_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SDA_BB_GPIO_Port, SDA_BB_Pin, GPIO_PIN_SET);

    /* ToF control: sensor held in reset until firmware is ready */
    HAL_GPIO_WritePin(TOF_LPN_GPIO_Port,     TOF_LPN_Pin,     GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TOF_INT_GPIO_Port,     TOF_INT_Pin,     GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TOF_I2C_RST_GPIO_Port, TOF_I2C_RST_Pin, GPIO_PIN_RESET);

    /* -------------------------------------------------------------------- */
    /* SD_MODE — PB1, push-pull output                                      */
    /* -------------------------------------------------------------------- */
    GPIO_InitStruct.Pin   = SD_MODE_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SD_MODE_GPIO_Port, &GPIO_InitStruct);

    /* -------------------------------------------------------------------- */
    /* I2C bit-bang — PB8 SCL_BB, PB9 SDA_BB — open-drain outputs          */
    /* -------------------------------------------------------------------- */
    GPIO_InitStruct.Pin   = SCL_BB_Pin | SDA_BB_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;    /* board has external pull-ups  */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* -------------------------------------------------------------------- */
    /* ToF sensor control — PD1 TOF_LPN                                    */
    /* -------------------------------------------------------------------- */
    GPIO_InitStruct.Pin   = TOF_LPN_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TOF_LPN_GPIO_Port, &GPIO_InitStruct);

    /* -------------------------------------------------------------------- */
    /* ToF sensor control — PE0 TOF_INT, PE1 TOF_I2C_RST                   */
    /* -------------------------------------------------------------------- */
    GPIO_InitStruct.Pin   = TOF_INT_Pin | TOF_I2C_RST_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* -------------------------------------------------------------------- */
    /* Pushbuttons — PC4 BTN_TONE, PC6 BTN_SENS                            */
    /* Active-low: pressed = GND, released = 3.3 V via internal pull-up    */
    /* -------------------------------------------------------------------- */
    GPIO_InitStruct.Pin  = BTN_TONE_Pin | BTN_SENS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* -------------------------------------------------------------------- */
    /* Scroll encoder — PD8 SCROLL_1 (CLK/A), PD9 SCROLL_NO (push-SW)     */
    /* -------------------------------------------------------------------- */
    GPIO_InitStruct.Pin  = SCROLL_1_Pin | SCROLL_NO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* -------------------------------------------------------------------- */
    /* Scroll encoder — PA9 SCROLL_2 (DT/B)                                */
    /* -------------------------------------------------------------------- */
    GPIO_InitStruct.Pin  = SCROLL_2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SCROLL_2_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE BEGIN 2 */
/* USER CODE END 2 */
