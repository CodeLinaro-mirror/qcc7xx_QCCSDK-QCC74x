################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Application/Startup/startup_stm32h7s3l8hx.s 

OBJS += \
./Application/Startup/startup_stm32h7s3l8hx.o 

S_DEPS += \
./Application/Startup/startup_stm32h7s3l8hx.d 


# Each subdirectory must supply rules for building sources it contributes
Application/Startup/%.o: ../Application/Startup/%.s Application/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m7 -g3 -DDEBUG -c -I../../../Appli/Inc -I../../../iperf -I../../../RCP -I../../../RCP/modem/include -I../../../RCP/transport/sdio/include -I../../../RCP/stream/wifi/include -I../../../RCP/stream/net/include -I../../../RCP/stream/bt/include -I../../../RCP/stream/tty/include -I../../../rcp_protocol/include -I../../../rcp_protocol/include/port -I../../../rcp_protocol/include/wifi -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc/Legacy -I../../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../../../Drivers/CMSIS/RTOS2/Include -I../../../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../../../Middlewares/Third_Party/LwIP/src/apps/http -I../../../Drivers/CMSIS/Device/ST/STM32H7RSxx/Include -I../../../Middlewares/Third_Party/LwIP/src/include/lwip -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../../../Middlewares/Third_Party/LwIP/src/include/netif -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../../../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../../../Middlewares/Third_Party/LwIP/system/arch -I../../../Drivers/CMSIS/Include -I../../../BleApp/App -I../../../BleApp/Target -I../../../Middlewares/ST/STM32_WPAN/ble/include -I../../../Middlewares/ST/STM32_WPAN/ble/include/auto -I../../../Middlewares/ST/STM32_WPAN/ble/Interfaces -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=soft -mthumb -o "$@" "$<"

clean: clean-Application-2f-Startup

clean-Application-2f-Startup:
	-$(RM) ./Application/Startup/startup_stm32h7s3l8hx.d ./Application/Startup/startup_stm32h7s3l8hx.o

.PHONY: clean-Application-2f-Startup

