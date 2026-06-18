sdk_generate_library()
							
sdk_add_include_directories(.)
sdk_add_include_directories(inc)

sdk_library_add_sources(
src/qcc74xsync_ble.c
src/encrypt_layer.c
src/payload.c
src/transfer.c
qcc74xsync_ble_app.c
)
