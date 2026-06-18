SDK_DEMO_PATH ?= $(abspath .)
QCC74x_SDK_BASE ?= ./../../../

export QCC74x_SDK_BASE

CHIP ?= qcc743
BOARD ?= qcc743dk
CROSS_COMPILE ?= riscv64-unknown-elf-

CONFIG_USE_LPAPP ?= y
cmake_definition+=-DCONFIG_USE_LPAPP=$(CONFIG_USE_LPAPP)
CONFIG_SDIO_OOB ?= n
cmake_definition+=-DCONFIG_SDIO_OOB=$(CONFIG_SDIO_OOB)

CONFIG_ANTENNA_CONTROL ?= y

include $(QCC74x_SDK_BASE)/project.build
