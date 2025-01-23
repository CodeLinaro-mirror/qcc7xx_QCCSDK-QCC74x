sdk_generate_library()

sdk_add_include_directories(include)

sdk_library_add_sources(src/nxspi.c)
sdk_library_add_sources(src/nxspi_flash.c)
sdk_library_add_sources(src/nxspi_port.c)
sdk_library_add_sources(src/nxspi_mem.c)
sdk_library_add_sources(src/nxspi_cli.c)
sdk_library_add_sources(src/nxspi_log.c)


