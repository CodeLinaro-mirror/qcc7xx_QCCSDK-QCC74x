cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)
#sdk_add_compile_definitions(-DQCC743_DHCP_DEBUG)


sdk_add_compile_definitions(-DLWIP_NETIF_EXT_STATUS_CALLBACK)

sdk_add_link_libraries(c m stdc++)

file(GLOB OBJECT_FILES "lib/*.o")
sdk_add_static_library(${OBJECT_FILES})

sdk_add_static_library(lib/libCHIP.a)
sdk_add_static_library(lib/chip-qcc743-lighting-example.out.a)
sdk_add_static_library(lib/libqrcode-common.a)
sdk_add_static_library(lib/libCHIPAppServer.a)

message(STATUS "Adding static library: ${CMAKE_CURRENT_SOURCE_DIR}/lib/chip-qcc743-lighting-example.out.a")

sdk_add_include_directories(.)

sdk_set_main_file(main.c)

project(matter_thread)
