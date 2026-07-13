# config coredump

CONFIG_COREDUMP=0, mean disable COREDUMP
default: CONFIG_COREDUMP=1, mean dump all memory to uart
CONFIG_COREDUMP=2, mean dump only key memory to flash, only when the remaining size of the FW partition exceeds 16KB, there is space to store it, otherwise this feature is turned off.
CONFIG_COREDUMP=3, mean enable dump key memory to flash and all memory to uart, only when the remaining size of the FW partition exceeds 16KB, there is space to store it, otherwise this feature is turned off.

For whole usage step, please refer to README.md.
