cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

target_sources(app PRIVATE jpeg_head.c)
sdk_set_main_file(main.c)

project(mjpeg_cam_normal)
