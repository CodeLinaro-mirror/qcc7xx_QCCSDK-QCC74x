cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

#sdk_add_compile_definitions(-DTICKLESS_DEBUG)
# sdk_add_compile_definitions(-DCONFIG_IRQ_USE_VECTOR)
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)

target_sources(app PRIVATE
                   wl_lp_app.c)

sdk_set_main_file(main.c)
sdk_add_include_directories(.)

project(wl_lp)
