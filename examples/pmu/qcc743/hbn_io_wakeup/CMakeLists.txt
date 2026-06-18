cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

sdk_add_compile_definitions(-DPM_HBN_LDO_LEVEL_DEFAULT=10)
sdk_add_compile_definitions(-DQCC74x_WIFI_LP_FW)
sdk_add_compile_definitions(-DLPFW_LOG_EN=0)

sdk_set_main_file(main.c)

project(hbn_io_wakeup)
