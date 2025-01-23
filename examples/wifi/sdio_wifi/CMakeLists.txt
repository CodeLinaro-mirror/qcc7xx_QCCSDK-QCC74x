cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

#sdk_add_compile_definitions(-DTICKLESS_DEBUG)
# sdk_add_compile_definitions(-DCONFIG_IRQ_USE_VECTOR)
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)

target_sources(app PRIVATE
                   app/src/app_sdiowifi.c
                   app/src/pm/app_pm.c
                   app/src/sample/app_sample.c
                   )

#sdio
if(CONFIG_HOSTROUTER)
    sdk_add_compile_definitions(-DQCC74x_HOSTROUTER_ENABLE)
    sdk_add_compile_definitions(-DQCC74x_SDIO_RX_MAX_PORT_NUM=4)
    sdk_add_compile_definitions(-DQCC74x_DUAL_STACK)
    #sdk_add_compile_definitions(-DCONFIG_SDIO_HIGH_PERFORMANCE)

    if(CONFIG_SDIOWIFI_HEARTBEAT)
        sdk_add_compile_definitions(-DSDIOWIFI_HEARTBEAT=1)
    endif()

    if(CONFIG_SDIOWIFI_TTY)
        sdk_add_compile_definitions(-DSDIOWIFI_TTY=1)
    endif()

    if(CONFIG_SDIO_TEST_NO_HOST)
        sdk_add_compile_definitions(-DSDIO_TEST_NO_HOST=1)
    endif()

    target_sources(app PRIVATE
                    app/src/rnm_user_ext/app_rnm_user.c
                    )
endif()

sdk_set_main_file(main.c)
sdk_add_include_directories(.)
sdk_add_include_directories(./app/include)

project(sdio_wifi)
