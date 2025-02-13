add_library(ds18b20 INTERFACE)

add_subdirectory(onewire_library)

target_sources(ds18b20 INTERFACE ds18b20.c)

target_link_libraries(ds18b20 INTERFACE pico_stdlib hardware_pio
                                        onewire_library)
