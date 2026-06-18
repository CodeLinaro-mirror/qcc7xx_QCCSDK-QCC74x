SDK_DEMO_PATH ?= $(abspath .)
QCC74x_SDK_BASE ?= ./../../../..

export QCC74x_SDK_BASE

CHIP ?= qcc743
BOARD ?= qcc743dk
CROSS_COMPILE ?= riscv64-unknown-elf-

# add custom cmake definition
#cmake_definition+=-Dxxx=sss

CONFIG_CSIDMA_CLI ?= 0
cmake_definition+=-DCONFIG_CSIDMA_CLI=$(CONFIG_CSIDMA_CLI)

include $(QCC74x_SDK_BASE)/project.build
