if(NOT DEFINED ENV{QCC74x_SDK_BASE})
    message( "please set QCC74x_SDK_BASE in your system environment")
endif()

if(MINGW OR CYGWIN OR WIN32)
SET(CMAKE_SYSTEM_NAME Generic)
elseif(APPLE)
SET(CMAKE_SYSTEM_NAME Darwin)
elseif(UNIX)
SET(CMAKE_SYSTEM_NAME Linux)
endif()
SET(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR RISCV)

set(QCC74x_SDK_BASE $ENV{QCC74x_SDK_BASE})

set(build_dir ${CMAKE_CURRENT_BINARY_DIR}/build_out)
set(PROJECT_SOURCE_DIR ${QCC74x_SDK_BASE})
set(PROJECT_BINARY_DIR ${build_dir})
set(EXECUTABLE_OUTPUT_PATH ${build_dir})
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

include(${CMAKE_CURRENT_BINARY_DIR}/generated/defconfig.cmake)

add_library(sdk_intf_lib INTERFACE)
add_library(app STATIC)
target_link_libraries(app sdk_intf_lib)

include(${QCC74x_SDK_BASE}/cmake/toolchain.cmake)
include(${QCC74x_SDK_BASE}/cmake/extension.cmake)
include(${QCC74x_SDK_BASE}/cmake/compiler_flags.cmake)

enable_language(C CXX ASM)

add_subdirectory(${QCC74x_SDK_BASE} ${build_dir})
