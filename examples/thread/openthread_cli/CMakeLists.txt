cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

string(TOLOWER "${CHIP}" lower_case_chip)
string(TOUPPER "${CHIP}" upper_case_chip)

sdk_add_compile_definitions(-DOPENTHREAD_CONFIG_PLATFORM_INFO=\"${upper_case_chip}\")

if(CONFIG_LMAC154_LOG)
sdk_add_compile_definitions(-DAPP_PRINT=lmac154_log)
else()
sdk_add_compile_definitions(-DAPP_PRINT=printf)
endif()

sdk_add_compile_definitions(-DOPENTHREAD_PROJECT_CORE_CONFIG_FILE=\"openthread-core-proj-config.h\")
sdk_add_compile_definitions(-DOPENTHREAD_CORE_CONFIG_PLATFORM_CHECK_FILE=\"openthread-core-platform-config-check.h\")

sdk_add_include_directories(.)

sdk_set_main_file(main.c)

project(openthread_cli)
