cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# Define
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)

target_sources(app PRIVATE
                   wifi_ota_test.c
                   )

sdk_add_include_directories(.)

if((${CHIP} STREQUAL "qcc743"))
    sdk_add_compile_definitions(-DCHIP_QCC743)
endif()

sdk_set_main_file(main.c)

project(wifi_ota)
