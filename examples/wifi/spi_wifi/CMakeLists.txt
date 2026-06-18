cmake_minimum_required(VERSION 3.15)

find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

if (DEFINED CONFIG_OT_RCP_ENABLE)
    message(WARNING "CONFIG_OT_RCP_ENABLE has been removed. Use CONFIG_OT_RCP=y instead.")
endif()

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

sdk_add_compile_definitions(-DMQTT_USE_CUSTOM_SOCKET_HANDLE=1)
sdk_add_compile_definitions(-DSHELL_THREAD_STACK_SIZE=512)

sdk_add_include_directories(.)
sdk_add_include_directories(./app/include)

sdk_set_main_file(main.c)

if(CONFIG_LWIP_ONHOST_ENABLE)
    sdk_add_compile_definitions(-DLWIP_HEAP_SIZE=\(4*1024\))
    sdk_add_compile_definitions(-DCONFIG_LWIP_ONHOST_IMAGE_VER="_lwip_onhost")
endif()

if(CONFIG_OT_NCP OR CONFIG_OT_RCP)
    sdk_add_compile_definitions(-DOPENTHREAD_PROJECT_CORE_CONFIG_FILE=\"openthread-core-proj-config.h\")
    sdk_add_compile_definitions(-DOPENTHREAD_CORE_CONFIG_PLATFORM_CHECK_FILE=\"openthread-core-platform-config-check.h\")
endif()

project(spi_wifi)
