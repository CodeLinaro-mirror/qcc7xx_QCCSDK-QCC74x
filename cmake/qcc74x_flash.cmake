if("${CMAKE_SYSTEM_NAME}" STREQUAL "Generic")
set(TOOL_SUFFIX ".exe")
set(CMAKE ${QCC74x_SDK_BASE}/tools/cmake/bin/cmake.exe)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
set(TOOL_SUFFIX "-ubuntu")
set(CMAKE cmake)
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
set(TOOL_SUFFIX "-macos")
set(CMAKE cmake)
endif()

if(EXISTS "${QCC74x_SDK_BASE}/VERSION")
    file(READ "${QCC74x_SDK_BASE}/VERSION" VERSION_FILE_CONTENT)

    string(REGEX MATCH "PROJECT_SDK_VERSION[ ]+\"([^\"]+)\"" _ ${VERSION_FILE_CONTENT})
    set(PROJECT_SDK_VERSION "${CMAKE_MATCH_1}")
else()
    set(PROJECT_SDK_VERSION "2.0.0")
endif()

set(QCC74x_FW_POST_PROC ${QCC74x_SDK_BASE}/tools/qcc74x_tools/QConn_Secure/QConn_Secure${TOOL_SUFFIX})

set(QCC74x_FW_POST_PROC_CONFIG --imgfile=${BIN_FILE} --appkeys=shared --ota_header=${PROJECT_SDK_VERSION})

if(BOARD_DIR)
list(APPEND QCC74x_FW_POST_PROC_CONFIG --brdcfgdir=${BOARD_DIR}/${BOARD}/config)
elseif(CONFIG_CUSTOMIZE_BOARD_CONFIG)
list(APPEND QCC74x_FW_POST_PROC_CONFIG --brdcfgdir=${QCC74x_SDK_BASE}/bsp/board/${BOARD}/${CONFIG_CUSTOMIZE_BOARD_CONFIG})
else()
list(APPEND QCC74x_FW_POST_PROC_CONFIG --brdcfgdir=${QCC74x_SDK_BASE}/bsp/board/${BOARD}/config)
endif()

if(CONFIG_AES_KEY)
list(APPEND QCC74x_FW_POST_PROC_CONFIG --key=${CONFIG_AES_KEY})
endif()

if(CONFIG_AES_IV)
list(APPEND QCC74x_FW_POST_PROC_CONFIG --iv=${CONFIG_AES_IV})
endif()

if(CONFIG_PUBLIC_KEY)
list(APPEND QCC74x_FW_POST_PROC_CONFIG --publickey=${CONFIG_PUBLIC_KEY})
endif()

if(CONFIG_PRIVATE_KEY)
list(APPEND QCC74x_FW_POST_PROC_CONFIG --privatekey=${CONFIG_PRIVATE_KEY})
endif()

if(CONFIG_FW_POST_PROC_CUSTOM)
list(APPEND QCC74x_FW_POST_PROC_CONFIG ${CONFIG_FW_POST_PROC_CUSTOM})
endif()

add_custom_target(combine
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${QCC74x_FW_POST_PROC} ${QCC74x_FW_POST_PROC_CONFIG})

file(GLOB OLD_MFG_BIN "${QCC74x_SDK_BASE}/bsp/board/${BOARD}/config/mfg*.bin")
file(GLOB DTS_FILES "${QCC74x_SDK_BASE}/bsp/board/${BOARD}/config/*.dts")
file(GLOB BOOT2_BIN_FILES "${QCC74x_SDK_BASE}/bsp/board/${BOARD}/config/boot2*.bin")
file(GLOB INI_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.ini")
set(PARTITION_BIN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/build/build_out/partition.bin")

set(post_build_cmds)
foreach(item ${CONFIG_POST_BUILDS})
    if("${item}" STREQUAL "RELEASE_MFG")
    list(APPEND post_build_cmds COMMAND ${CMAKE} -E echo "[release] generate mfg_release directory"
                            COMMAND ${CMAKE} -E remove ${OLD_MFG_BIN}
                            COMMAND ${CMAKE} -E copy ${BIN_FILE} ${QCC74x_SDK_BASE}/bsp/board/${BOARD}/config/mfg_${CHIP}_tmpver.bin
                            COMMAND ${CMAKE} -E remove_directory mfg_release
                            COMMAND ${CMAKE} -E copy_directory ${QCC74x_SDK_BASE}/tools/qcc74x_tools mfg_release
                            COMMAND ${CMAKE} -E copy ${BIN_FILE} ${PARTITION_BIN_FILES} ${BOOT2_BIN_FILES} ${DTS_FILES} ${INI_FILES} mfg_release)
    endif()

    if("${item}" STREQUAL "CONCAT_WITH_LP_FW")
    list(APPEND post_build_cmds COMMAND ${CMAKE} -E echo "[lp_fw] concate with lp fw bin"
                                COMMAND ${QCC74x_SDK_BASE}/tools/lpfw/patch_lpfw${TOOL_SUFFIX} ${BIN_FILE} ${QCC74x_SDK_BASE}/tools/lpfw/bin/${CHIP}_lp_fw.bin)
    endif()

    if("${item}" STREQUAL "GENERATE_ROMFS")
    list(APPEND post_build_cmds COMMAND ${CMAKE} -E echo "[romfs] generate romfs.bin using romfs directory"
                                COMMAND ${QCC74x_SDK_BASE}/tools/genromfs/genromfs${TOOL_SUFFIX} -d romfs/ -f ./build/build_out/romfs.bin)
    endif()

    if("${item}" STREQUAL "GENERATE_LITTLEFS")
    list(APPEND post_build_cmds COMMAND ${CMAKE} -E echo "[littlefs] generate littlefs.bin using littlefs directory"
                                COMMAND ${QCC74x_SDK_BASE}/tools/genlfs/mklfs${TOOL_SUFFIX} -c lfs -b 4096 -p 256 -r 256 -s 0x71000 -i ./build/build_out/littlefs.bin)
    endif()

endforeach()

add_custom_target(post_build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        ${post_build_cmds})

file(GLOB BUILD_BINS "${CMAKE_CURRENT_SOURCE_DIR}/build/build_out/*.bin" "${CMAKE_CURRENT_SOURCE_DIR}/build/build_out/*.elf" "${CMAKE_CURRENT_SOURCE_DIR}/build/build_out/*.xz" "${CMAKE_CURRENT_SOURCE_DIR}/build/build_out/*.ota")

set(pre_build_cmds)
if(BUILD_BINS)
    list(APPEND pre_build_cmds COMMAND ${CMAKE} -E echo "[clean] clean old binary"
                               COMMAND ${CMAKE} -E remove ${BUILD_BINS})
endif()
add_custom_target(pre_build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        ${pre_build_cmds})
