cmake_minimum_required(VERSION 3.15)

# check VERSION file
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    # read info from VERSION file
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_FILE_CONTENT)

    string(REGEX MATCH "PROJECT_SDK_VERSION[ ]+\"([^\"]+)\"" _ ${VERSION_FILE_CONTENT})
    set(PROJECT_SDK_VERSION "${CMAKE_MATCH_1}")
else()
     execute_process(
         COMMAND git describe --tags --abbrev=0
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
         OUTPUT_VARIABLE GIT_TAG
         OUTPUT_STRIP_TRAILING_WHITESPACE
         ERROR_QUIET
     )

     if(GIT_TAG)
         set(PROJECT_SDK_VERSION ${GIT_TAG})
     else()
         set(PROJECT_SDK_VERSION "2.0.0")
     endif()
endif()
message(STATUS "Project SDK Version: ${PROJECT_SDK_VERSION}")

# handle __FILE__ macro issue
set(SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
sdk_add_compile_options("-fmacro-prefix-map=${SOURCE_PATH}=.")
sdk_add_compile_options("-fdebug-prefix-map=${SOURCE_PATH}=.")

if(CONFIG_ANTI_ROLLBACK)
    sdk_add_compile_definitions(-DCONFIG_ANTI_ROLLBACK)
endif()
if(CONFIG_XTAL_POWER_TYPE_ACTIVE)
    sdk_add_compile_definitions(-DCONFIG_XTAL_POWER_TYPE_ACTIVE)
endif()
sdk_add_compile_definitions(-DCONFIG_PROJECT_PARAM="${CONFIG_PROJECT_PARAM}")
sdk_add_compile_definitions(-DCONFIG_APP_ANTI_ROLLBACK_VER=${CONFIG_APP_ANTI_ROLLBACK_VER})

if(CONFIG_BSP_CONSOLE_USB_CDC)
    set(USB_CONSOLE_MISSING_CONFIGS "")

    foreach(required_config
            CONFIG_FREERTOS
            CONFIG_SHELL
            CONFIG_CHERRYUSB
            CONFIG_CHERRYUSB_DEVICE
            CONFIG_CHERRYUSB_DEVICE_CDC_ACM)
        if(NOT DEFINED ${required_config} OR NOT "${${required_config}}" STREQUAL "y")
            list(APPEND USB_CONSOLE_MISSING_CONFIGS "${required_config}=y")
        endif()
    endforeach()

    if(NOT "${CONFIG_CHERRYUSB_OSAL}" STREQUAL "freertos")
        list(APPEND USB_CONSOLE_MISSING_CONFIGS "CONFIG_CHERRYUSB_OSAL=freertos")
    endif()

    if(USB_CONSOLE_MISSING_CONFIGS)
        list(JOIN USB_CONSOLE_MISSING_CONFIGS "\n  " USB_CONSOLE_MISSING_CONFIGS_TEXT)
        message(FATAL_ERROR
            "CONFIG_BSP_CONSOLE_USB_CDC=y requires the following defconfig settings:\n"
            "  ${USB_CONSOLE_MISSING_CONFIGS_TEXT}")
    endif()
endif()

add_subdirectory(bsp)
add_subdirectory(components)
add_subdirectory(drivers/lhal)
add_subdirectory(drivers/soc/${CHIP}/std)
if("${CHIP}" STREQUAL "qcc743")
add_subdirectory(drivers/sys)
endif()
sdk_add_subdirectory_ifdef(CONFIG_RF drivers/soc/${CHIP}/phyrf)
if("${CHIP}" STREQUAL "qcc743")
sdk_add_subdirectory_ifdef(CONFIG_RF drivers/rfparam)
sdk_add_subdirectory_ifdef(CONFIG_PHY_LPFW drivers/soc/qcc743/phyrf)
endif()
