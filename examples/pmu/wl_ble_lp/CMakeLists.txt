cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

if (CONFIG_USBH_MSC)
target_sources(app PRIVATE
    usbh_msc_fatfs.c
              )
endif()

target_sources(app PRIVATE
    wl_ble_lp_app.c
              )

sdk_add_include_directories(.)

sdk_set_main_file(main.c)

project(ble_lp)
