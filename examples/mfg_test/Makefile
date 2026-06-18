SDK_DEMO_PATH ?= $(abspath .)
QCC74x_SDK_BASE ?= ./../..

export QCC74x_SDK_BASE

CHIP ?= qcc743
BOARD ?= qcc743dk
CROSS_COMPILE ?= riscv64-unknown-elf-

CONFIG_POST_BUILDS_RELEASE_MFG ?=y

DISABLE_MFG_AUTOBOOT ?= 0
cmake_definition+=-DDISABLE_MFG_AUTOBOOT=$(DISABLE_MFG_AUTOBOOT)

# add custom cmake definition
#cmake_definition+=-Dxxx=sss

include $(QCC74x_SDK_BASE)/project.build
