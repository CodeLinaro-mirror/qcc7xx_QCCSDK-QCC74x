cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# sdk_add_compile_definitions(-D SDIO2_DEBUG)

sdk_set_main_file(main.c)

project(sdio2_test)
