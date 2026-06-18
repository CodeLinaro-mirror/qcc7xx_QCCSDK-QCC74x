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

# fw tools board directory select
get_property(FW_TOOL_BOARD_DIR GLOBAL PROPERTY FW_TOOL_CUSTOM_BOARD_DIR)
if(FW_TOOL_BOARD_DIR)
    message(STATUS "Select FW_TOOL_CUSTOM_BOARD_DIR: ${FW_TOOL_BOARD_DIR}")
else()
    set(FW_TOOL_BOARD_DIR ${QCC74x_SDK_BASE}/bsp/board/${BOARD})
    message(STATUS "Select BOARD: ${FW_TOOL_BOARD_DIR}")
endif()
# fw tools board config select
get_property(FW_TOOL_BOARD_CONFIG GLOBAL PROPERTY FW_TOOL_CUSTOM_BOARD_CONFIG)
if(FW_TOOL_BOARD_CONFIG)
    message(STATUS "Select FW_TOOL_CUSTOM_BOARD_CONFIG: ${FW_TOOL_BOARD_CONFIG}")
else()
    set(FW_TOOL_BOARD_CONFIG config)
    message(STATUS "Select FW_TOOL_BOARD_CONFIG: ${FW_TOOL_BOARD_CONFIG}")
endif()

if(CONFIG_OVERRIDE_PTFILE)

    set(PTFILE_PATH_DEMO "${SDK_DEMO_PATH}/${CONFIG_OVERRIDE_PTFILE}")
    set(PTFILE_PATH_DEFAULT "${FW_TOOL_BOARD_DIR}/${CONFIG_OVERRIDE_PTFILE}")
    
    set(FOUND_PTFILE_PATH "") 

    if(EXISTS "${PTFILE_PATH_DEMO}")
        set(FOUND_PTFILE_PATH "${PTFILE_PATH_DEMO}")
    elseif(EXISTS "${PTFILE_PATH_DEFAULT}")
        set(FOUND_PTFILE_PATH "${PTFILE_PATH_DEFAULT}")
    else()
        message(FATAL_ERROR "'${CONFIG_OVERRIDE_PTFILE}' was not found at '${PTFILE_PATH_DEMO}' or '${PTFILE_PATH_DEFAULT}'.")
    endif()

    if(NOT "${FOUND_PTFILE_PATH}" STREQUAL "")
        list(APPEND QCC74x_FW_POST_PROC_CONFIG --ptfile=${FOUND_PTFILE_PATH})
    endif()
endif()

if(CONFIG_OVERRIDE_DTSFILE)
    list(APPEND QCC74x_FW_POST_PROC_CONFIG --dtsfile=${SDK_DEMO_PATH}/${CONFIG_OVERRIDE_DTSFILE})
endif()

if(CONFIG_OVERRIDE_BOOT2FILE)
    list(APPEND QCC74x_FW_POST_PROC_CONFIG --boot2file=${SDK_DEMO_PATH}/${CONFIG_OVERRIDE_BOOT2FILE})
endif()

if(CONFIG_OVERRIDE_MFGFILE)
    list(APPEND QCC74x_FW_POST_PROC_CONFIG --mfgfile=${SDK_DEMO_PATH}/${CONFIG_OVERRIDE_MFGFILE})
endif()

# fw tools --brdcfgdir select
list(APPEND QCC74x_FW_POST_PROC_CONFIG --brdcfgdir=${FW_TOOL_BOARD_DIR}/${FW_TOOL_BOARD_CONFIG})

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

set(combine_cmds)

if(CONFIG_POST_BUILDS_CONCAT_WITH_LP_FW)
list(APPEND combine_cmds COMMAND ${CMAKE} -E echo "[lp_fw] concate with lp fw bin"
                         COMMAND ${QCC74x_SDK_BASE}/tools/lpfw/patch_lpfw${TOOL_SUFFIX} ${BIN_FILE} ${QCC74x_SDK_BASE}/tools/lpfw/bin/${CHIP}_lp_fw.bin)
endif()

list(APPEND combine_cmds COMMAND ${QCC74x_FW_POST_PROC} ${QCC74x_FW_POST_PROC_CONFIG})

add_custom_target(combine
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        ${combine_cmds})

file(GLOB OLD_MFG_BIN "${QCC74x_SDK_BASE}/bsp/board/${BOARD}/config/mfg*.bin")
file(GLOB DTS_FILES "${QCC74x_SDK_BASE}/bsp/board/${BOARD}/config/*.dts")
file(GLOB BOOT2_BIN_FILES "${QCC74x_SDK_BASE}/bsp/board/${BOARD}/config/boot2*.bin")
file(GLOB INI_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.ini")
set(PARTITION_BIN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_DIR}/build_out/partition.bin")

if(CONFIG_POST_BUILDS_RELEASE_MFG)
list(APPEND post_build_cmds COMMAND ${CMAKE} -E echo "[release] generate mfg_release directory"
                        COMMAND ${CMAKE} -E remove ${OLD_MFG_BIN}
                        COMMAND ${CMAKE} -E copy ${BIN_FILE} ${QCC74x_SDK_BASE}/bsp/board/${BOARD}/config/mfg_${CHIP}_tmpver.bin
                        COMMAND ${CMAKE} -E remove_directory mfg_release
                        COMMAND ${CMAKE} -E copy_directory ${QCC74x_SDK_BASE}/tools/qcc74x_tools mfg_release
                        COMMAND ${CMAKE} -E copy ${BIN_FILE} ${PARTITION_BIN_FILES} ${BOOT2_BIN_FILES} ${DTS_FILES} ${INI_FILES} mfg_release)
endif()

if(CONFIG_POST_BUILDS_GENERATE_ROMFS)
list(APPEND post_build_cmds COMMAND ${CMAKE} -E echo "[romfs] generate romfs.bin using romfs directory"
                            COMMAND ${QCC74x_SDK_BASE}/tools/genromfs/genromfs${TOOL_SUFFIX} -d romfs/ -f ./${BUILD_DIR}/build_out/romfs.bin)
endif()

if(CONFIG_POST_BUILDS_GENERATE_LITTLEFS)
list(APPEND post_build_cmds COMMAND ${CMAKE} -E echo "[littlefs] generate littlefs.bin using littlefs directory"
                            COMMAND ${QCC74x_SDK_BASE}/tools/genlfs/mklfs${TOOL_SUFFIX} -c lfs -b 4096 -p 256 -r 256 -s 0x6d000 -i ./${BUILD_DIR}/build_out/littlefs.bin)
endif()

add_custom_target(post_build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        ${post_build_cmds})

file(GLOB BUILD_BINS "${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_DIR}/build_out/*.bin" "${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_DIR}/build_out/*.elf" "${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_DIR}/build_out/*.xz" "${CMAKE_CURRENT_SOURCE_DIR}/${BUILD_DIR}/build_out/*.ota")

set(pre_build_cmds)
if(BUILD_BINS)
    list(APPEND pre_build_cmds COMMAND ${CMAKE} -E echo "[clean] clean old binary"
                               COMMAND ${CMAKE} -E remove ${BUILD_BINS})
endif()
add_custom_target(pre_build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        ${pre_build_cmds})
