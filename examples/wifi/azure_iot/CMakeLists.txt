cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)
#sdk_add_compile_definitions(-DQCC743_DHCP_DEBUG)

sdk_add_include_directories(.)

sdk_set_main_file(main.c
)

include_directories(iot-middleware-freertos-samples/demos/common/transport)
include_directories(iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/interface)
include_directories(iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/include)
include_directories(iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/ports/coreMQTT)
include_directories(iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/ports/coreHTTP)
include_directories(iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/inc/)
include_directories(iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/coreMQTT/source/include)
include_directories(iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/coreHTTP/source/dependency/3rdparty/http_parser)
include_directories(iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/coreHTTP/source/include)
include_directories(iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/coreHTTP/source/interface)

target_sources(app PRIVATE
transport_socket.c
transport_tls_socket_using_mbedtls.c
iot-middleware-freertos-samples/demos/common/utilities/mbedtls_freertos_port.c
iot-middleware-freertos-samples/demos/common/transport/sockets_wrapper_lwip.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/azure_iot.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/azure_iot_message.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/azure_iot_hub_client.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/azure_iot_hub_client_properties.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/azure_iot_adu_client.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/azure_iot_provisioning_client.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/azure_iot_json_reader.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/source/azure_iot_json_writer.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/ports/coreMQTT/azure_iot_core_mqtt.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/ports/mbedTLS/azure_iot_jws_mbedtls.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/ports/coreHTTP/azure_iot_core_http.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/core/az_json_reader.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/core/az_context.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/core/az_log.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/core/az_precondition.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/core/az_json_writer.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/core/az_json_token.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/core/az_span.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/core/az_base64.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_hub_client_methods.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_hub_client.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_adu_client.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_hub_client_telemetry.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_hub_client_twin.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_hub_client_sas.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_hub_client_properties.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_hub_client_commands.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_hub_client_c2d.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_adu_client.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_provisioning_client_sas.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_provisioning_client.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/azure-sdk-for-c/sdk/src/azure/iot/az_iot_common.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/coreHTTP/source/core_http_client.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/coreHTTP/source/dependency/3rdparty/http_parser/http_parser.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/coreMQTT/source/core_mqtt_serializer.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/coreMQTT/source/core_mqtt.c
iot-middleware-freertos-samples/libs/azure-iot-middleware-freertos/libraries/coreMQTT/source/core_mqtt_state.c
)

project(azure_iot)
