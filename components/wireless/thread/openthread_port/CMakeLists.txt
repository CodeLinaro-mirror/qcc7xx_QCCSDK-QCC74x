sdk_generate_library()

# ==================== Deprecated variable warnings ====================

if (DEFINED OT_NCP)
  message(WARNING "OT_NCP is deprecated. Use CONFIG_OT_NCP or CONFIG_OT_RCP instead.")
endif()
if (DEFINED OT_SERIAL)
  message(WARNING "OT_SERIAL is deprecated. Use CONFIG_OT_CLI instead (value: 'shell' or 'uart').")
endif()
if (DEFINED OT_FTD)
  message(WARNING "OT_FTD is deprecated. Use CONFIG_OT_FTD instead.")
endif()
if (DEFINED OT_MTD)
  message(WARNING "OT_MTD is deprecated. Use CONFIG_OT_MTD instead.")
endif()

# ==================== CONFIG_OT_CLI value validation ====================

if (CONFIG_OT_CLI)
  if (NOT ("${CONFIG_OT_CLI}" STREQUAL "shell" OR "${CONFIG_OT_CLI}" STREQUAL "uart"))
    message(FATAL_ERROR "CONFIG_OT_CLI must be 'shell' or 'uart', got '${CONFIG_OT_CLI}'.")
  endif()
endif()

# ==================== CONFIG_NXSPI_OPENTHREAD_RADIO usage check ====================

if (CONFIG_NXSPI_OPENTHREAD_RADIO AND NOT (CONFIG_OT_NCP OR CONFIG_OT_RCP))
  message(WARNING "CONFIG_NXSPI_OPENTHREAD_RADIO only takes effect when CONFIG_OT_NCP or CONFIG_OT_RCP is enabled.")
endif()

string(TOUPPER "${CHIP}" upper_case_chip)
string(TOLOWER "${CHIP}" lower_case_chip)

sdk_add_private_compile_definitions(-D${upper_case_chip})
sdk_add_private_compile_definitions(-DCHIP_HDR=<${lower_case_chip}.h>)
sdk_add_private_compile_definitions(-DCHIP_GLB_HDR=<${lower_case_chip}_glb.h>)
sdk_add_private_compile_definitions(-DCHIP_SYS_HDR=<${lower_case_chip}_sys.h>)
sdk_add_private_compile_definitions(-DCONFIG_QCC74x_SDK)

if ("${upper_case_chip}" STREQUAL "QCC743")
  sdk_add_private_compile_definitions(-DMAC_ADDRESS_MAX_NUM=3)
else ()
  sdk_add_private_compile_definitions(-DMAC_ADDRESS_MAX_NUM=2)
endif()

sdk_add_include_directories(./include)

if (CONFIG_LITTLEFS)
  sdk_library_add_sources(./ot_settings_littlefs.c)
elseif (CONFIG_EASYFLASH4)
  sdk_library_add_sources(./ot_settings_easyflash.c)
  message(WARNING "Easyflash is deprecated. Better to use littlefs/CONFIG_LITTLEFS for faster storage access.")
else ()
  message(FATAL_ERROR "Please select one of CONFIG_LITTLEFS and CONFIG_EASYFLASH4 for OpenThread setting storage.")
endif()

if (NOT CONFIG_MATTER)
  sdk_library_add_sources(./ot_freertos.c)
endif()

sdk_library_add_sources(./ot_sys_qcc74x.c)
sdk_library_add_sources(./ot_alarm_${lower_case_chip}.c)
sdk_library_add_sources(./ot_radio.c)
sdk_library_add_sources(./ot_radio_qcc74x.c)

if (CONFIG_OTBR)
  sdk_add_private_include_directories(.)
  sdk_library_add_sources(./otbr_lwip.c)
endif()

if ("${CONFIG_OT_CLI}" STREQUAL "shell")
  sdk_library_add_sources(./ot_shell.c)
elseif ("${CONFIG_OT_CLI}" STREQUAL "uart") 
  sdk_library_add_sources(./ot_uart_qcc74x.c)
  if (OT_SERIAL_BUFFER_SIZE)
    sdk_add_private_compile_definitions(-DOT_SERIAL_BUFFER_SIZE=${OT_SERIAL_BUFFER_SIZE})
  endif()
  if (OT_SERIAL_LINE_SIZE)
    sdk_add_private_compile_definitions(-DOT_SERIAL_LINE_SIZE=${OT_SERIAL_LINE_SIZE})
  endif()
endif()

if(CONFIG_OT_NCP OR CONFIG_OT_RCP)
  if(CONFIG_OT_CLI)
    message(FATAL_ERROR "CONFIG_OT_CLI can't be enabled with CONFIG_OT_NCP or CONFIG_OT_RCP.")
  endif()
  if(CONFIG_OT_RCP)
    sdk_add_compile_definitions(-DOPENTHREAD_RADIO=1)
  else()
    sdk_add_compile_definitions(-DOPENTHREAD_RADIO=0)
  endif()
  if(CONFIG_NXSPI_OPENTHREAD_RADIO)
    sdk_library_add_sources(./ot_nxspi.c)
  else()
    sdk_library_add_sources(./ot_uart_qcc74x.c)
  endif()
else()
  sdk_library_add_sources(./ot_logging.c)
endif()

if (CONFIG_OT_FTD)
  sdk_add_compile_definitions(-DOPENTHREAD_FTD=1)
else()
  sdk_add_compile_definitions(-DOPENTHREAD_FTD=0)
endif()
if (CONFIG_OT_MTD)
  sdk_add_compile_definitions(-DOPENTHREAD_MTD=1)
else()
  sdk_add_compile_definitions(-DOPENTHREAD_MTD=0)
endif()

if(CONFIG_OT_FTD AND CONFIG_OT_MTD)
    message(FATAL_ERROR "Can not enable CONFIG_OT_FTD and CONFIG_OT_MTD together")
endif()

if(CONFIG_OT_NCP AND CONFIG_OT_RCP)
    message(FATAL_ERROR "Can not enable CONFIG_OT_NCP and CONFIG_OT_RCP together")
endif()

# disable undef warning
# sdk_add_compile_options(-Wno-undef)
