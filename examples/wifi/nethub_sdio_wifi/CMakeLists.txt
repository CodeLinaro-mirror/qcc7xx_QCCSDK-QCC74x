cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)

sdk_add_compile_definitions(-DCONFIG_ATMODULE=1)
sdk_add_compile_definitions(-DCONFIG_ATMODULE_TTY=1)
sdk_add_compile_definitions(-DCONFIG_ATMODULE_WIFI=1)

sdk_add_compile_definitions(-DCONFIG_NETHUB=1)

target_sources(app PRIVATE
                   app/src/atmodule/app_atmodule.c
                   app/src/app_user.c
                   )

sdk_add_include_directories(.)
sdk_add_include_directories(./app/include)

sdk_set_main_file(main.c)

project(nethub_sdio_wifi)
