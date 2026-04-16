option(RCP_STM_LWIP "lwIP network drivers" OFF)
option(RCP_STM_BT "BT HCI drivers" OFF)

add_library(rcp_stm STATIC)

target_include_directories(rcp_stm PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/Include
)

target_sources(rcp_stm PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/Src/qcc74x_sdio.c
	${CMAKE_CURRENT_LIST_DIR}/Src/rcp_bus_sdio_master.c
	${CMAKE_CURRENT_LIST_DIR}/Src/sdio_hal.c
)

if (RCP_STM_LWIP)

target_sources(rcp_stm PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/Src/rcp_data_test.c
	${CMAKE_CURRENT_LIST_DIR}/Src/rcp_lwip_netif.c
	${CMAKE_CURRENT_LIST_DIR}/Src/rcp_mcu_capability.c
	${CMAKE_CURRENT_LIST_DIR}/Src/lwip_sys.c
)
	
endif()

if (RCP_STM_BT)

target_sources(rcp_stm PRIVATE 
	${CMAKE_CURRENT_LIST_DIR}/Src/rcp_bt_hci.c
)

endif()
