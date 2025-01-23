sdk_add_include_directories(.)

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/board.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fw_header.c)

if(CONFIG_RF)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/board_rf.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wireless_regdb.c)
endif()

if(CONFIG_LINK_FOR_MFGTEST)
sdk_set_linker_script(qcc743_chip_mfgtest.ld)
sdk_add_compile_definitions(-DCONFIG_MFG)
elseif(CONFIG_LPAPP)
sdk_set_linker_script(qcc743_lp.ld)
sdk_add_link_options(
-L ${CMAKE_CURRENT_SOURCE_DIR}
)
elseif(CONFIG_WFA)
sdk_set_linker_script(qcc743_wfa_flash.ld)
sdk_add_link_options(
-L ${CMAKE_CURRENT_SOURCE_DIR}
)
else()
sdk_set_linker_script(qcc743_flash.ld)
endif()

if(CONFIG_PSRAM_HEAP)
message(FATAL_ERROR "
FATAL: qcc743dk donot have psram, please disable CONFIG_PSRAM_HEAP.
")
endif()
if(CONFIG_PSRAM_COPY_CODE)
message(FATAL_ERROR "
FATAL: qcc743dk donot have psram, please disable CONFIG_PSRAM_COPY_CODE.
")
endif()
if(CONFIG_CONSOLE_WO)
sdk_add_compile_definitions(-DCONFIG_CONSOLE_WO)
endif()

sdk_add_compile_definitions_ifdef(CONFIG_ANTENNA_CONTROL -DCONFIG_ANTENNA_CONTROL)
sdk_add_compile_definitions_ifdef(CONFIG_PSRAM_COPY_CODE -DCONFIG_PSRAM_COPY_CODE)

sdk_add_link_options(-ufw_header)

sdk_add_link_options(-Wl,--build-id)
