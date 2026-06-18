################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

C_SRCS += \
../../../RCP/transport/sdio/master/qcc74x_sdio.c \
../../../RCP/transport/sdio/master/rcp_bus_sdio_master.c

OBJS += \
./RCP/transport/sdio/master/qcc74x_sdio.o \
./RCP/transport/sdio/master/rcp_bus_sdio_master.o

C_DEPS += \
./RCP/transport/sdio/master/qcc74x_sdio.d \
./RCP/transport/sdio/master/rcp_bus_sdio_master.d


# Each subdirectory must supply rules for building sources it contributes
RCP/transport/sdio/master/qcc74x_sdio.o: ../../../RCP/transport/sdio/master/qcc74x_sdio.c RCP/transport/sdio/master/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32H7S3xx -c -I../../../Appli/Inc -I../../../iperf -I../../../RCP -I../../../RCP/modem/include -I../../../RCP/transport/sdio/include -I../../../RCP/stream/wifi/include -I../../../RCP/stream/net/include -I../../../RCP/stream/bt/include -I../../../RCP/stream/tty/include -I../../../rcp_protocol/include -I../../../rcp_protocol/include/port -I../../../rcp_protocol/include/wifi -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc/Legacy -I../../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../../../Drivers/CMSIS/RTOS2/Include -I../../../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../../../Middlewares/Third_Party/LwIP/src/apps/http -I../../../Drivers/CMSIS/Device/ST/STM32H7RSxx/Include -I../../../Middlewares/Third_Party/LwIP/src/include/lwip -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../../../Middlewares/Third_Party/LwIP/src/include/netif -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../../../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../../../Middlewares/Third_Party/LwIP/system/arch -I../../../Drivers/CMSIS/Include -I../../../BleApp/App -I../../../BleApp/Target -I../../../Middlewares/ST/STM32_WPAN/ble/include -I../../../Middlewares/ST/STM32_WPAN/ble/include/auto -I../../../Middlewares/ST/STM32_WPAN/ble/Interfaces -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=soft -mthumb -o "$@"
RCP/transport/sdio/master/rcp_bus_sdio_master.o: ../../../RCP/transport/sdio/master/rcp_bus_sdio_master.c RCP/transport/sdio/master/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32H7S3xx -c -I../../../Appli/Inc -I../../../iperf -I../../../RCP -I../../../RCP/modem/include -I../../../RCP/transport/sdio/include -I../../../RCP/stream/wifi/include -I../../../RCP/stream/net/include -I../../../RCP/stream/bt/include -I../../../RCP/stream/tty/include -I../../../rcp_protocol/include -I../../../rcp_protocol/include/port -I../../../rcp_protocol/include/wifi -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc/Legacy -I../../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../../../Drivers/CMSIS/RTOS2/Include -I../../../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../../../Middlewares/Third_Party/LwIP/src/apps/http -I../../../Drivers/CMSIS/Device/ST/STM32H7RSxx/Include -I../../../Middlewares/Third_Party/LwIP/src/include/lwip -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../../../Middlewares/Third_Party/LwIP/src/include/netif -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../../../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../../../Middlewares/Third_Party/LwIP/system/arch -I../../../Drivers/CMSIS/Include -I../../../BleApp/App -I../../../BleApp/Target -I../../../Middlewares/ST/STM32_WPAN/ble/include -I../../../Middlewares/ST/STM32_WPAN/ble/include/auto -I../../../Middlewares/ST/STM32_WPAN/ble/Interfaces -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=soft -mthumb -o "$@"

clean: clean-RCP-2f-transport-2f-sdio-2f-master

clean-RCP-2f-transport-2f-sdio-2f-master:
	-$(RM) ./RCP/transport/sdio/master/qcc74x_sdio.cyclo ./RCP/transport/sdio/master/qcc74x_sdio.d ./RCP/transport/sdio/master/qcc74x_sdio.o ./RCP/transport/sdio/master/qcc74x_sdio.su ./RCP/transport/sdio/master/rcp_bus_sdio_master.cyclo ./RCP/transport/sdio/master/rcp_bus_sdio_master.d ./RCP/transport/sdio/master/rcp_bus_sdio_master.o ./RCP/transport/sdio/master/rcp_bus_sdio_master.su

.PHONY: clean-RCP-2f-transport-2f-sdio-2f-master
