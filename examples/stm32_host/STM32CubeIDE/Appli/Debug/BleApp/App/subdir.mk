################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../../../BleApp/App/app_ble.c \
../../../BleApp/App/lhci.c 

OBJS += \
./BleApp/App/app_ble.o \
./BleApp/App/lhci.o 

C_DEPS += \
./BleApp/App/app_ble.d \
./BleApp/App/lhci.d 


# Each subdirectory must supply rules for building sources it contributes
BleApp/App/app_ble.o: ../../../BleApp/App/app_ble.c BleApp/App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32H7S3xx -c -I../../../Appli/Inc -I../../../iperf -I../../../RCP -I../../../RCP/modem/include -I../../../RCP/transport/sdio/include -I../../../RCP/stream/wifi/include -I../../../RCP/stream/net/include -I../../../RCP/stream/bt/include -I../../../RCP/stream/tty/include -I../../../rcp_protocol/include -I../../../rcp_protocol/include/port -I../../../rcp_protocol/include/wifi -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc/Legacy -I../../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../../../Drivers/CMSIS/RTOS2/Include -I../../../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../../../Middlewares/Third_Party/LwIP/src/apps/http -I../../../Drivers/CMSIS/Device/ST/STM32H7RSxx/Include -I../../../Middlewares/Third_Party/LwIP/src/include/lwip -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../../../Middlewares/Third_Party/LwIP/src/include/netif -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../../../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../../../Middlewares/Third_Party/LwIP/system/arch -I../../../Drivers/CMSIS/Include -I../../../BleApp/App -I../../../BleApp/Target -I../../../Middlewares/ST/STM32_WPAN/ble/include -I../../../Middlewares/ST/STM32_WPAN/ble/include/auto -I../../../Middlewares/ST/STM32_WPAN/ble/Interfaces -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=soft -mthumb -o "$@"
BleApp/App/lhci.o: ../../../BleApp/App/lhci.c BleApp/App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32H7S3xx -c -I../../../Appli/Inc -I../../../iperf -I../../../RCP -I../../../RCP/modem/include -I../../../RCP/transport/sdio/include -I../../../RCP/stream/wifi/include -I../../../RCP/stream/net/include -I../../../RCP/stream/bt/include -I../../../RCP/stream/tty/include -I../../../rcp_protocol/include -I../../../rcp_protocol/include/port -I../../../rcp_protocol/include/wifi -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc/Legacy -I../../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../../../Drivers/CMSIS/RTOS2/Include -I../../../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../../../Middlewares/Third_Party/LwIP/src/apps/http -I../../../Drivers/CMSIS/Device/ST/STM32H7RSxx/Include -I../../../Middlewares/Third_Party/LwIP/src/include/lwip -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../../../Middlewares/Third_Party/LwIP/src/include/netif -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../../../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../../../Middlewares/Third_Party/LwIP/system/arch -I../../../Drivers/CMSIS/Include -I../../../BleApp/App -I../../../BleApp/Target -I../../../Middlewares/ST/STM32_WPAN/ble/include -I../../../Middlewares/ST/STM32_WPAN/ble/include/auto -I../../../Middlewares/ST/STM32_WPAN/ble/Interfaces -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=soft -mthumb -o "$@"

clean: clean-BleApp-2f-App

clean-BleApp-2f-App:
	-$(RM) ./BleApp/App/app_ble.cyclo ./BleApp/App/app_ble.d ./BleApp/App/app_ble.o ./BleApp/App/app_ble.su ./BleApp/App/lhci.cyclo ./BleApp/App/lhci.d ./BleApp/App/lhci.o ./BleApp/App/lhci.su

.PHONY: clean-BleApp-2f-App

