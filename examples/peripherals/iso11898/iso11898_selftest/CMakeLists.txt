cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

target_link_directories(app PUBLIC ../driver)
sdk_add_link_libraries(iso11898)
sdk_add_include_directories(../driver)

sdk_set_main_file(main.c)

project(iso11898_selftest)
