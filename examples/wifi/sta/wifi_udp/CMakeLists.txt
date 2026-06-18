cmake_minimum_required(VERSION 3.15)
find_package(qcc74x_sdk REQUIRED HINTS $ENV{QCC74x_SDK_BASE})

# User
sdk_add_compile_definitions(-DCONFIG_CLI_CMD_ENABLE)
#sdk_add_compile_definitions(-DQCC743_DHCP_DEBUG)

if(CONFIG_WIFI_GATEWAY)
#sdk_add_compile_definitions(-DFHOST_BUFFER_COPY=1)
#sdk_add_compile_definitions(-DFHOST_RX_BUF_CNT=48)
#sdk_add_compile_definitions(-DFHOST_RX_BUF_SECTION=\".bss\")
#sdk_add_compile_definitions(-DCFG_ENABLE_AP_PS)
#sdk_add_compile_definitions(-DLWIP_HEAP_SIZE=\(32*1024\))
sdk_add_compile_definitions(-DIP_FORWARD=1)
sdk_add_compile_definitions(-DIP_NAPT=1)
sdk_add_compile_definitions(-DCFG_IP_NAPT_TIMEOUT_MS_TCP=\(30*60*1000\))
sdk_add_compile_definitions(-DCFG_IP_NAPT_TIMEOUT_MS_TCP_DISCON=\(2*20*1000\))
sdk_add_compile_definitions(-DCFG_IP_NAPT_TIMEOUT_MS_UDP=\(620*1000\))
sdk_add_compile_definitions(-DCFG_IP_NAPT_TIMEOUT_MS_ICMP=\(5*2*1000\))
endif()

if(CONFIG_MDNS)
sdk_add_compile_definitions(-DCFG_MDNS)
endif()

target_sources(app PRIVATE
                   wifi_udp_echo.c)

sdk_add_include_directories(.)

sdk_set_main_file(main.c)

project(wifi_udp)
