cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

sdk_add_include_directories(. ./mqttc_adapter)
sdk_add_include_directories(. ./wifi_adapter)

sdk_add_compile_definitions(-DMQTT_USE_CUSTOM_SOCKET_HANDLE=1)

target_sources(app PRIVATE
               app/app_ble.c
               app/app_wifi.c
               app/app_mqtt.c
               mqttc_adapter/mqttc.c
               wifi_adapter/wifi_event.c
               )

sdk_set_main_file(main.c)

project(wifi_mqtt_event)
