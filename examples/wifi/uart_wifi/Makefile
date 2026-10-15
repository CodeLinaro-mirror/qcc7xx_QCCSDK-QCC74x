SDK_DEMO_PATH ?= $(abspath .)
QCC74x_SDK_BASE ?= ./../../../

export QCC74x_SDK_BASE

CHIP ?= qcc743
BOARD ?= qcc743dk
CROSS_COMPILE ?= riscv64-unknown-elf-

# add custom cmake definition
#cmake_definition+=-Dxxx=sss

CONFIG_LWIP_ONHOST_ENABLE ?= n
CONFIG_USE_LPAPP ?= n
CONFIG_BLUETOOTH_APP ?= y
CONFIG_BLE_HOST_CHECK ?= y
CONFIG_HCI_SPI_ENABLE ?= n
ifeq ($(CONFIG_HCI_SPI_ENABLE), y)
    ifneq ($(CONFIG_LWIP_ONHOST_ENABLE), y)
        $(error "Bluetooth HCI mode only runs with Wi-Fi Lwip On Host mode.")
    endif
    ifeq ($(CONFIG_BLUETOOTH_APP), y)
        $(error "BLE AT CMD NOT run Bluetooth HCI mode.")
    endif
endif

ifeq ($(CONFIG_LWIP_ONHOST_ENABLE), y)
CONFIG_IPV6 = n
else
CONFIG_IPV6 ?= y
endif

CONFIG_MQTT ?= y
CONFIG_HTTP ?= y
CONFIG_NETWORK ?= y
CONFIG_ANTENNA_CONTROL ?= y
CONFIG_UART_WIFI_GPIO_OVERLAY ?= n

include $(QCC74x_SDK_BASE)/project.build 
