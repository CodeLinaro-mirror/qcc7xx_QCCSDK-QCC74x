cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

target_sources(app PRIVATE ../foc.c)

sdk_set_main_file(main.c)

project(pwm_foc_hall)
