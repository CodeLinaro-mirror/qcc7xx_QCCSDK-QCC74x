SDK_DEMO_PATH ?= $(abspath .)
QCC74x_SDK_BASE ?= ./../../..

export QCC74x_SDK_BASE

CHIP ?= qcc743
BOARD ?= ${CHIP}dk
CROSS_COMPILE ?= riscv64-unknown-elf-

CONFIG_OT_NCP ?= n
CONFIG_OT_RCP ?= n
CONFIG_OT_FTD ?= n
CONFIG_OT_MTD ?= n

include $(QCC74x_SDK_BASE)/project.build
