cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)
#sdk_add_compile_definitions(-DCONFIG_MEMSET_OPTSPEED)

# Enable coexistence timeslice statistics debug


if (CONFIG_LPAPP)
    target_sources(app PRIVATE
                       app/src/app_spiwifi.c
                       app/src/atmoudle/app_atmoudle.c
                       app/src/pm/app_pm.c
                       )
else ()

target_sources(app PRIVATE
                   app/src/app_spiwifi.c
                   app/src/atmoudle/app_atmoudle.c
                   # app/src/easyflash4/app_easyflash4.c
                   # app/src/atcmd/app_atcmd.c
                   # app/src/sample/app_sample.c
                   # app/src/spisync/app_spisync.c
                   # app/src/system/app_msgbuffer.c
                   # app/src/useruart/app_useruart.c
                   )
endif()

sdk_add_compile_definitions(-DAT_NET_TX_MAX_LEN=NXBD_MTU)
sdk_add_compile_definitions(-DAT_TRANS_ZEROCOPY=1)

if (CONFIG_SPI_3PIN_MODE_ENABLE)
    sdk_add_compile_definitions(-DCONFIG_SPI_3PIN_MODE_ENABLE=1)
endif()

if (CONFIG_AT_DEBUG_ENABLE)
    sdk_add_compile_definitions(-DCONFIG_AT_DEBUG_ENABLE=1)
endif()

sdk_add_compile_definitions(-DMQTT_USE_CUSTOM_SOCKET_HANDLE=1)
sdk_add_compile_definitions(-DSHELL_THREAD_STACK_SIZE=512)

if (CONFIG_IPV6)
    sdk_add_compile_definitions(-DCONFIG_IPV6=1)
endif()

sdk_add_include_directories(.)
sdk_add_include_directories(./app/include)

sdk_set_main_file(main.c)

if (CONFIG_LWIP_ONHOST_ENABLE)
sdk_add_compile_definitions(-DLWIP_HEAP_SIZE=\(4*1024\))
sdk_add_compile_definitions(-DCONFIG_LWIP_ONHOST_ENABLE=$(CONFIG_LWIP_ONHOST_ENABLE))
sdk_add_compile_definitions(-DCONFIG_LWIP_ONHOST_IMAGE_VER="_lwip_onhost")
endif()

if (DEFINED OT_NCP AND NOT OT_NCP)
    sdk_add_compile_definitions(-DNXSPI_OPENTHREAD_RADIO=1)
else()
    sdk_add_compile_definitions(-DNXSPI_OPENTHREAD_RADIO=0)
endif()

project(spi_wifi)
