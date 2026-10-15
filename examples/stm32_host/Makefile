default:
	mkdir -p $(BUILD_DIR)
	cp -a STM32CubeIDE/Appli/Debug STM32CubeIDE/Appli/$(BUILD_DIR)
	make -C STM32CubeIDE/Appli/$(BUILD_DIR)/ stm32_host_Appli.elf
	arm-none-eabi-objcopy -O binary STM32CubeIDE/Appli/$(BUILD_DIR)/stm32_host_Appli.elf $(BUILD_DIR)/stm32_flash_data.bin
	rm -rf STM32CubeIDE/Appli/$(BUILD_DIR)
