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

# Print the resolved per-rate TX power (value + source: user table / calib
# table) on every tpc_powercntrlinfo_get() call. See tpc.c. Off by default;
# build with CONFIG_WL_BB_TPC_LOG=1 to enable.
CONFIG_WL_BB_TPC_LOG ?= 0
cmake_definition+=-DWL_BB_TPC_LOG=$(CONFIG_WL_BB_TPC_LOG)

include $(QCC74x_SDK_BASE)/project.build
