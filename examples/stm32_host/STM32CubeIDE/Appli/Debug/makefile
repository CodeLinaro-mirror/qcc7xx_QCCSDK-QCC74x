################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include rcp_protocol/port/perf/subdir.mk
-include rcp_protocol/port/os/freertos/subdir.mk
-include rcp_protocol/src/subdir.mk
-include iperf/subdir.mk
-include RCP/subdir.mk
-include RCP/modem/src/subdir.mk
-include RCP/stream/wifi/src/subdir.mk
-include RCP/stream/net/src/subdir.mk
-include RCP/stream/bt/src/subdir.mk
-include RCP/stream/tty/src/subdir.mk
-include RCP/transport/sdio/master/subdir.mk
-include RCP/transport/sdio/hal/stm32h7/subdir.mk
-include Middlewares/ST/STM32_WPAN/ble/Interfaces/subdir.mk
-include Middlewares/LwIP/subdir.mk
-include Middlewares/FreeRTOS/subdir.mk
-include Drivers/STM32H7RSxx_HAL_Driver/subdir.mk
-include Drivers/CMSIS/subdir.mk
-include BleApp/Target/subdir.mk
-include BleApp/App/subdir.mk
-include Application/User/subdir.mk
-include Application/Startup/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := stm32_host_Appli
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
stm32_host_Appli.elf \

MAP_FILES += \
stm32_host_Appli.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
stm32_host_Appli.list \


# All Target
all: main-build

# Main-build Target
main-build: stm32_host_Appli.elf secondary-outputs

# Tool invocations
stm32_host_Appli.elf stm32_host_Appli.map: $(OBJS) $(USER_OBJS) ../STM32H7S3L8HX_ROMxspi2_app.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "stm32_host_Appli.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m7 -T"../STM32H7S3L8HX_ROMxspi2_app.ld" --specs=nosys.specs -Wl,-Map="stm32_host_Appli.map" -Wl,--gc-sections -static -L"../../../Middlewares/ST/STM32_WPAN/ble/lib" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=soft -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

stm32_host_Appli.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "stm32_host_Appli.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) default.size.stdout stm32_host_Appli.elf stm32_host_Appli.list stm32_host_Appli.map
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
