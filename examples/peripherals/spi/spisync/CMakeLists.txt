cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)
sdk_add_compile_definitions(-DCONFIG_HIGH_PERFORMANCE=1)

if(NOT CONFIG_LINK_IN_PSRAM)
sdk_add_compile_definitions(-DCFG_LINK_ALL_IN_FLASH=1)
endif()

#sdk_add_compile_definitions(-DQCC743_DHCP_DEBUG)

target_sources(app PRIVATE
                   app/src/app_spisync.c
                   )

sdk_add_include_directories(.)
sdk_add_include_directories(./app/include)

sdk_set_main_file(main.c)

project(spisync)
