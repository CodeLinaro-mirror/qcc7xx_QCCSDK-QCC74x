cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

sdk_add_include_directories(.)

target_sources(app PRIVATE
                qcc74x_eflash_loader_interface.c
                qcc74x_eflash_loader_cmds.c
                qcc74x_eflash_loader_uart.c
                qcc74xsp_boot_decompress.c
                qcc74xsp_boot_parser.c
                qcc74xsp_common.c
                qcc74xsp_media_boot.c
                qcc74xsp_port.c
                qcc74xsp_ram_image_boot.c
                partition.c
                ${CMAKE_SOURCE_DIR}/port/${CHIP}/qcc74x_port_boot2.c
                )


if("${CHIP}" STREQUAL "qcc743")
target_sources(app PRIVATE rv32i_xtheade_lz4.S)
endif()

if("${CHIP}" STREQUAL "qcc74x_undef")
target_sources(app PRIVATE rv32i_xtheade_lz4.S)
endif()

if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/port/${CHIP})
include_directories(${CMAKE_SOURCE_DIR}/port/${CHIP})
if(EXISTS "${CMAKE_SOURCE_DIR}/port/${CHIP}/CMakeLists.txt")
    add_subdirectory(${CMAKE_SOURCE_DIR}/port/${CHIP})
endif()
else()
message(FATAL_ERROR "${CMAKE_SOURCE_DIR}/port/${CHIP} is not exist")
endif()
include_directories(${CMAKE_SOURCE_DIR})
sdk_set_main_file(main.c)
sdk_set_linker_script(port/${CHIP}/qcc74xsp_boot2_iap_flash.ld)

if("${CONFIG_DEBUG}" STREQUAL "y")
sdk_add_compile_definitions(-DCONFIG_DEBUG)
endif()
sdk_add_compile_definitions(-DQCC74x_BOOT2_VER="${CONFIG_BOOT2_VER}")
sdk_add_compile_definitions(-DQCC74x_BOOT2)
sdk_add_compile_definitions(-DCONFIG_TRAP_DUMP_DISABLE)
message(${CONFIG_BOOT2_VER})

if(CONFIG_BOOT2)
    sdk_add_compile_definitions(-DCONFIG_BOOT2)
endif()
sdk_add_compile_definitions(-DBOOT2_ANTI_ROLLBACK_VER=${CONFIG_BOOT2_ANTI_ROLLBACK_VER})

if((${CHIP} STREQUAL "qcc74x_undef"))
    sdk_add_compile_definitions(-DCHIP_QCC74x_undef)
endif()

if((${CHIP} STREQUAL "qcc74x_undef"))
    sdk_add_compile_definitions(-DCHIP_QCC74x_undef)
endif()

if((${CHIP} STREQUAL "qcc74x_undef"))
    sdk_add_compile_definitions(-DCHIP_QCC74x_undef)
endif()

if((${CHIP} STREQUAL "qcc74x_undef"))
    sdk_add_compile_definitions(-DCHIP_QCC74x_undef)
    sdk_add_compile_definitions(-DCONFIG_SUPPORT_UART_CLOCK_GET_ONLY)
    sdk_add_compile_definitions(-D CONIFG_DISABLE_ALL_PMP_EXCEPT_PMPADDR0)
    sdk_add_compile_definitions(-D CONIFG_DISABLE_ALL_CLOCK_GET_EXCEPT_SYSTEM_CLOCK)
    sdk_add_compile_definitions(-D CONIFG_DISABLE_ALL_UHS_EXCEPT_40M)
    sdk_add_compile_definitions(-D CONIFG_DISABLE_ALL_PSRAM_FRQ_EXCEPT_2000)
    sdk_add_compile_definitions(-D CONIFG_DISABLE_INTERRUPT_BEHADN_UART0)
    sdk_add_compile_definitions(-D CONIFG_DISABLE_OTHER_CORE_EXCEPT_M0)
    sdk_add_compile_definitions(-D CONFIG_NOT_SUPPORT_0X13_0X14_0X15_0X16_FLASH)
endif()

if((${CHIP} STREQUAL "qcc74x_undef"))
    sdk_add_compile_definitions(-DCHIP_QCC74x_undef)
    sdk_add_compile_definitions(-DCONFIG_SUPPORT_UART_CLOCK_GET_ONLY)
    sdk_add_compile_definitions(-D CONIFG_DISABLE_ALL_CLOCK_GET_EXCEPT_SYSTEM_CLOCK)
    sdk_add_compile_definitions(-D CONFIG_NOT_SUPPORT_0X13_0X14_0X15_0X16_FLASH)
endif()

if((${CHIP} STREQUAL "qcc743"))
    sdk_add_compile_definitions(-DCHIP_QCC743)
endif()
sdk_add_compile_definitions(-D CONIFG_DISABLE_NOCACHE_RAM_LOAD)
sdk_add_compile_definitions(-D CONFIG_DISABLE_SBUS2_ENABLE_SUPPORT)
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_definitions(-DGIT_COMMIT="${GIT_COMMIT}")

project(boot2_isp)
