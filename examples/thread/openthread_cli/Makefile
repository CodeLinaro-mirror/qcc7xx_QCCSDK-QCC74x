SDK_DEMO_PATH ?= $(abspath .)
QCC74x_SDK_BASE ?= ./../../..

export QCC74x_SDK_BASE

CHIP ?= qcc743
BOARD ?= qcc743dk
CROSS_COMPILE ?= riscv64-unknown-elf-

CONFIG_LMAC154_CMD ?= n
CONFIG_LMAC154_LOG ?= n

CONFIG_OT_FTD ?= y
CONFIG_OT_MTD ?= n
CONFIG_OT_CLI ?= shell
# CONFIG_OT_CLI ?= uart

ifeq ("$(CONFIG_OT_CLI)", "shell")
  CONFIG_SHELL =y
  CONFIG_SHELL_CMD_SIZE =256
endif

include $(QCC74x_SDK_BASE)/project.build
