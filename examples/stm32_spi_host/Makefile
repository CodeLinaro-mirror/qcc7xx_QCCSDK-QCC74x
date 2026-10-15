default:
	mkdir -p $(BUILD_DIR)
	cp -a QCC743_SPI_HOST/Debug QCC743_SPI_HOST/$(BUILD_DIR)
	make -C QCC743_SPI_HOST/$(BUILD_DIR)/ QCC743_SPI_HOST.elf
	arm-none-eabi-objcopy -O binary QCC743_SPI_HOST/$(BUILD_DIR)/QCC743_SPI_HOST.elf $(BUILD_DIR)/stm32_flash_data.bin
	rm -rf QCC743_SPI_HOST/$(BUILD_DIR)
