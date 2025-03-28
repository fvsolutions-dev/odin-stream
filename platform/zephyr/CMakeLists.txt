zephyr_library_named(odin-stream)
zephyr_library_sources(
"../../src/streaming_packet.c"
"../../src/parameter_set.c"
)

target_include_directories(odin-stream PUBLIC ../../src)

