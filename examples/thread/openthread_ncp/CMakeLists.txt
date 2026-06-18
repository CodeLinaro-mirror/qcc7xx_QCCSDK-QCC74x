cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

string(TOUPPER "${CHIP}" upper_case_chip)
string(TOLOWER "${CHIP}" lower_case_chip)
sdk_add_compile_definitions(-DOPENTHREAD_CONFIG_PLATFORM_INFO=\"${upper_case_chip}\")

sdk_add_compile_definitions(-DOT_SERIAL_BUFFER_SIZE=1024)
sdk_add_compile_definitions(-DOT_SERIAL_LINE_SIZE=256)
sdk_add_compile_definitions(-DOPENTHREAD_PROJECT_CORE_CONFIG_FILE=\"openthread-core-proj-config.h\")
sdk_add_compile_definitions(-DOPENTHREAD_CORE_CONFIG_PLATFORM_CHECK_FILE=\"openthread-core-platform-config-check.h\")

sdk_add_include_directories(.)

target_sources(app PRIVATE board_${lower_case_chip}.c)

sdk_set_main_file(main.c)

project(openthread_ncp)
