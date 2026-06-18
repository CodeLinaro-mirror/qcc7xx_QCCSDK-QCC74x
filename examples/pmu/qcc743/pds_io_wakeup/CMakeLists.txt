cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

sdk_add_compile_definitions(-DPM_PDS_LDO_LEVEL_DEFAULT=10)

sdk_set_main_file(main.c)

project(pds_io_wakeup)
