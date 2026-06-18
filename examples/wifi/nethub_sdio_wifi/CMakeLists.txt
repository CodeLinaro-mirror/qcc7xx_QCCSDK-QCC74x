cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)

sdk_add_compile_definitions(-DCONFIG_ATMODULE=1)
sdk_add_compile_definitions(-DCONFIG_ATMODULE_TTY=1)
sdk_add_compile_definitions(-DCONFIG_ATMODULE_WIFI=1)

sdk_add_compile_definitions(-DCONFIG_NETHUB=1)
sdk_add_compile_definitions(-DCONFIG_NXSDIO_HCI=1)
sdk_add_compile_definitions(-DLWIP_HEAP_SIZE=\(4*1024\))

if (CONFIG_LPAPP OR CONFIG_USE_LPAPP)
target_sources(app PRIVATE
                   app/src/atmodule/app_atmodule.c
                   app/src/app_user.c
                    app/src/pm/app_pm.c
                   )
else()
target_sources(app PRIVATE
                   app/src/atmodule/app_atmodule.c
                   app/src/app_user.c
                   )
endif()

sdk_add_include_directories(.)
sdk_add_include_directories(./app/include)

sdk_set_main_file(main.c)

# Use the project-local linker script that grows ram_lp by 18K to host
# the nh_msg_ctrl dnld frame pool, freeing ram_wifi for the rest.
if (CONFIG_LPAPP OR CONFIG_USE_LPAPP)
sdk_set_linker_script_macro(${CMAKE_CURRENT_SOURCE_DIR}/sdiowifi.ld)
endif()

project(nethub_sdio_wifi)
