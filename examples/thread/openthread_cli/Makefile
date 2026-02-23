SDK_DEMO_PATH ?= $(abspath .)
QCC74x_SDK_BASE ?= ./../../..

export QCC74x_SDK_BASE

CHIP ?= qcc743
BOARD ?= qcc743dk
CROSS_COMPILE ?= riscv64-unknown-elf-

OT_FTD ?= 1 
OT_SERIAL ?= SYS_CLI
CONFIG_LMAC154_CMD ?= 0
CONFIG_LMAC154_LOG ?= 0
CONFIG_WIFI6 ?= 0
CONFIG_BLUETOOTH ?= 0

cmake_definition+=-DOT_FTD=${OT_FTD}
cmake_definition+=-DOT_CLI=1

ifeq ("$(OT_SERIAL)", "SYS_CLI")
  cmake_definition+=-DCONFIG_SHELL_CMD_SIZE=256
  cmake_definition+=-DCONFIG_SHELL=1
endif
cmake_definition+=-DOT_SERIAL=${OT_SERIAL}

cmake_definition+=-DCONFIG_LMAC154_CMD=${CONFIG_LMAC154_CMD}
cmake_definition+=-DCONFIG_LMAC154_LOG=${CONFIG_LMAC154_LOG}

ifneq ("$(CONFIG_WIFI6)", "0")
  cmake_definition+=-DCONFIG_WIFI6=${CONFIG_WIFI6}
endif
ifneq ("$(CONFIG_BLUETOOTH)", "0")
  cmake_definition+=-DCONFIG_BLUETOOTH=${CONFIG_BLUETOOTH}
endif

cmake_definition+=-DOPENTHREAD_PROJECT_CORE_CONFIG_FILE=\"openthread-core-proj-config.h\"
cmake_definition+=-DOPENTHREAD_CORE_CONFIG_PLATFORM_CHECK_FILE=\"openthread-core-platform-config-check.h\"

include $(QCC74x_SDK_BASE)/project.build
