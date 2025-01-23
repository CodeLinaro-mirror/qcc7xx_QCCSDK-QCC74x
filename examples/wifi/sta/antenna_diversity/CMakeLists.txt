cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)

if(NOT CONFIG_PSRAM_COPY_CODE)
sdk_add_compile_definitions(-DCFG_LINK_ALL_IN_FLASH=1)
endif()

#sdk_add_compile_definitions(-DQCC743_DHCP_DEBUG)

target_sources(app PRIVATE)

sdk_add_include_directories(.)

sdk_set_main_file(main.c)

project(antenna_diversity)
