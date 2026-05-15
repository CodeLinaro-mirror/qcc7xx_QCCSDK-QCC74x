option(RCP_OS_ZEPHYR "Enable Zephyr OS bindings" OFF)
option(RCP_OS_LINUX "Enable Linux OS bindings" OFF)
option(RCP_OS_FREERTOS "Enable FreeRTOS OS bindings" OFF)
option(RCP_WIFI_MGMT "Enable generic RCP WiFi management module" OFF)

add_library(qualcomm_rcp_protocol
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_bus.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_interface_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_protocol.c
    ${CMAKE_CURRENT_LIST_DIR}/src/nh_credit_flow.c
    ${CMAKE_CURRENT_LIST_DIR}/src/buffer_mgmt_ctrl.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_mux_dispatcher.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_tx_scheduler.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_cntrl_msgs.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_error_codes.c
)

target_include_directories(qualcomm_rcp_protocol PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include/
)

if (RCP_OS_LINUX)

target_sources(qualcomm_rcp_protocol PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_os_linux.c
)

endif()

if (RCP_OS_ZEPHYR)

target_sources(qualcomm_rcp_protocol PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_os_zephyr.c
)

endif()

if (RCP_OS_FREERTOS)

target_sources(qualcomm_rcp_protocol PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/rcp_os_freertos.c
)

endif()

if (RCP_WIFI_MGMT)

target_sources(qualcomm_rcp_protocol PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/src/wifi/rcp_wifi_mgmt.c)

# nanopb
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/ext/nanopb/extra)
find_package(Nanopb REQUIRED)

set(PROTO_DIR ${CMAKE_CURRENT_LIST_DIR}/proto)

nanopb_generate_cpp(TARGET rcp_wifi_mgmt_proto RELPATH ${PROTO_DIR}
	${PROTO_DIR}/wifi_mgmt.proto
)

target_link_libraries(qualcomm_rcp_protocol PRIVATE rcp_wifi_mgmt_proto)

endif()
