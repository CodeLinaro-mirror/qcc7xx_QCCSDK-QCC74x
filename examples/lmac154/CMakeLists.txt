cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

sdk_add_include_directories(.)

target_sources(app PRIVATE lmac154_example.c)

sdk_set_main_file(main.c)

project(lmac154)
