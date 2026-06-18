sdk_generate_library()
sdk_library_add_sources(
src/easyflash.c
src/ef_env.c
src/ef_env_legacy_wl.c
src/ef_env_legacy.c
src/ef_port.c src/ef_utils.c
src/easyflash_cli.c
)

sdk_add_include_directories(inc)
