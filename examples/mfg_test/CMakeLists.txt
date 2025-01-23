cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)
sdk_add_compile_definitions(-DQCC74x_VERIFY)
sdk_add_compile_definitions(-DMFG_GU_QCC743)
sdk_add_compile_definitions(-DMFG_PSRAM_TEST)
sdk_add_compile_definitions(-DCONFIG_XTAL_SELECT_EN)

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_definitions(-DGIT_COMMIT="${GIT_COMMIT}")

if(DISABLE_MFG_AUTOBOOT)
add_definitions(-DDISABLE_MFG_AUTOBOOT)
endif()

sdk_add_compile_definitions(-DBUILD_ROM_CODE)

target_sources(app PRIVATE
                mfg_sdio.c
                mfg_sdu.c
                mfg_m154.c
                mfg_main.c
                mfg_atcmd.c
                mfg_atcmd_port.c
                mfg_otp.c
            )

sdk_add_include_directories(.)
sdk_add_include_directories(./build/generated/)

sdk_set_main_file(main.c)

project(mfg_test)
