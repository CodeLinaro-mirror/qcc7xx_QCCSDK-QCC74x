SDK_DEMO_PATH ?= $(abspath .)
QCC74x_SDK_BASE ?= ./../../../

export QCC74x_SDK_BASE

CHIP ?= qcc743
BOARD ?= qcc743dk
CROSS_COMPILE ?= riscv64-unknown-elf-

# add custom cmake definition
#cmake_definition+=-Dxxx=sss

CONFIG_LWIP_ONHOST_ENABLE ?= n
CONFIG_USE_LPAPP ?= y
CONFIG_BLUETOOTH_APP ?= y
CONFIG_BLE_HOST_CHECK ?= y
CONFIG_HCI_SPI_ENABLE ?= n
CONFIG_HCI_UART_ENABLE ?= n
ifeq ($(CONFIG_HCI_SPI_ENABLE)$(CONFIG_HCI_UART_ENABLE), yy)
    $(error "SPI and UART HCI cannot be enabled at the same time.")
endif
ifeq ($(or $(CONFIG_HCI_SPI_ENABLE),$(CONFIG_HCI_UART_ENABLE)), y)
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

CONFIG_OT_RCP ?= n
CONFIG_OT_NCP ?= n

CONFIG_MQTT ?= y
CONFIG_HTTP ?= y
CONFIG_NETWORK ?= y
CONFIG_ANTENNA_CONTROL ?= y
CONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN_16K ?= n

ifeq ($(CONFIG_AT_MDNS_ENABLE), y)
CONFIG_MDNS = y
endif

include $(QCC74x_SDK_BASE)/project.build 
