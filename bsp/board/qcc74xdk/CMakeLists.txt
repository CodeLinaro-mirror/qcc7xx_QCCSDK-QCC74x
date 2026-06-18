sdk_add_include_directories(.)

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/board_flash_psram.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/board_gpio.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/board.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fw_header.c)

if(CONFIG_RF)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/board_rf.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wireless_regdb.c)
endif()

if(CONFIG_CUSTOM_LINKSCRIPT)
message(STATUS "LINK IN CUSTOM_LINKSCRIPT")
elseif(CONFIG_LINK_FOR_MFGTEST)
sdk_set_linker_script(qcc743_chip_mfgtest.ld)
sdk_add_compile_definitions(-DCONFIG_MFG)
elseif(CONFIG_LPAPP)
    if(CONFIG_LWIP_ONHOST_ENABLE)
    sdk_set_linker_script_macro(qcc743_lp_rcp.ld.in)
    else()
    sdk_set_linker_script_macro(qcc743_lp_ncp.ld.in)
    endif()
sdk_add_link_options(
-L ${CMAKE_CURRENT_SOURCE_DIR}
)
elseif(CONFIG_WFA)
sdk_set_linker_script(qcc743_wfa_flash.ld)
sdk_add_link_options(
-L ${CMAKE_CURRENT_SOURCE_DIR}
)
else()
message(STATUS "LINK IN common.ld.in")
sdk_set_linker_script_macro(common.ld.in)
endif()

if(CONFIG_CONSOLE_WO)
sdk_add_compile_definitions(-DCONFIG_CONSOLE_WO)
endif()

if(CONFIG_PSRAM_COPY_CODE)
sdk_add_compile_definitions(-DCONFIG_PSRAM)
endif()

if(CONFIG_PSRAM)
sdk_add_compile_definitions(-DCONFIG_PSRAM)
endif()

if(CONFIG_CONSOLE_WO)
sdk_add_compile_definitions(-DCONFIG_CONSOLE_WO)
endif()

sdk_add_compile_definitions_ifdef(CONFIG_ANTENNA_CONTROL -DCONFIG_ANTENNA_CONTROL)
sdk_add_compile_definitions_ifdef(CONFIG_PSRAM_COPY_CODE -DCONFIG_PSRAM_COPY_CODE)

sdk_add_link_options(-ufw_header)

# fw tools board directory select
if(CONFIG_CUSTOMIZE_BOARD_DIR)
    set_property(GLOBAL PROPERTY FW_TOOL_CUSTOM_BOARD_DIR ${CONFIG_CUSTOMIZE_BOARD_DIR})
else()
    set_property(GLOBAL PROPERTY FW_TOOL_CUSTOM_BOARD_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

set_property(GLOBAL PROPERTY FW_TOOL_CUSTOM_BOARD_CONFIG config)
