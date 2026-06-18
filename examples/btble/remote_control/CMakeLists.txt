cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

sdk_add_include_directories(.)

target_sources(app PRIVATE 
    ble_rc_app.c
    ble_rc_hog.c
)

sdk_set_main_file(main.c)

project(remote_control)
