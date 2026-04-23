################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Sola/stm32ai-modelzoo-services/application_code/object_detection/STM32N6/STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.c 

OBJS += \
./Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.o 

C_DEPS += \
./Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.o: C:/Users/Sola/stm32ai-modelzoo-services/application_code/object_detection/STM32N6/STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.c Drivers/STM32N6xx_HAL_Driver/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m55 -std=gnu11 -g3 -DSTM32N657xx '-DECBLOB_CONST_SECTION=__attribute__((section(".flash_blob")))' -DSTM32N6570_NUCLEO_REV -DSCR_LIB_USE_SPI -DSCR_LIB_USE_NONE -DUSE_FULL_ASSERT -DUSE_FULL_LL_DRIVER -DVECT_TAB_SRAM -DLL_ATON_DUMP_DEBUG_API -DLL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6 -DLL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL -DLL_ATON_RT_MODE=LL_ATON_RT_ASYNC -DLL_ATON_SW_FALLBACK -DLL_ATON_DBG_BUFFER_INFO_EXCLUDED=1 -c -I../../Inc -I../../../../Middlewares/ai-postprocessing-wrapper -I../../../../Middlewares/stm32-vision-models-postprocessing/lib_vision_models_pp/Inc -I../../../../Middlewares/stedgeai-lib/Npu/ll_aton -I../../../../Middlewares/stedgeai-lib/Npu/Devices/STM32N6xx -I../../../../Model/NUCLEO-N657X0-Q -I../../../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Inc -I../../../../STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy -I../../../../STM32Cube_FW_N6/Drivers/CMSIS/Device/ST/STM32N6xx/Include -I../../../../STM32Cube_FW_N6/Drivers/CMSIS/Include -I../../../../STM32Cube_FW_N6/Drivers/CMSIS/DSP/Include -I../../../../STM32Cube_FW_N6/Drivers/BSP/Components/Common -I../../../../STM32Cube_FW_N6/Drivers/BSP/STM32N6xx_Nucleo -I../../../../Middlewares/stm32-mw-camera/ISP_Library/isp/Inc -I../../../../Middlewares/stedgeai-lib/Inc -I../../../../STM32Cube_FW_N6/Utilities/lcd -I../../../../Middlewares/stm32-mw-camera -I../../../../STM32Cube_FW_N6/Drivers/BSP/Components/aps256xx -I../../../../Middlewares/stm32-mw-camera/sensors -I../../../../Middlewares/stm32-mw-camera/sensors/imx335 -I../../../../Middlewares/stm32-mw-camera/sensors/vd6g -I../../../../Middlewares/stm32-mw-camera/sensors/vd55g1 -I../../../../Middlewares/stm32-mw-camera/sensors/vd1943 -I../../../../Middlewares/stm32-mw-camera/sensors/ov5640 -I../../../../STM32Cube_FW_N6/Middlewares/ST/usbx/common/core/inc -I../../../../STM32Cube_FW_N6/Middlewares/ST/usbx/ports/generic/inc -I../../../../STM32Cube_FW_N6/Middlewares/ST/usbx/common/usbx_device_classes/inc -I../../../../STM32Cube_FW_N6/Middlewares/ST/usbx/common/usbx_stm32_device_controllers -I../../../../Middlewares/screenl/Inc -I../../../../Middlewares/screenl/ili9341 -I../../../../Middlewares/screenl/uvcl/Inc -I../../../../Middlewares/screenl/uvcl/Src -I../../../../Middlewares/screenl/uvcl/Inc/usbx -I../../../../Middlewares/screenl/uvcl/Src/usbx -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -mcmse -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-STM32N6xx_HAL_Driver-2f-Src

clean-Drivers-2f-STM32N6xx_HAL_Driver-2f-Src:
	-$(RM) ./Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.cyclo ./Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.d ./Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.o ./Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.su

.PHONY: clean-Drivers-2f-STM32N6xx_HAL_Driver-2f-Src

