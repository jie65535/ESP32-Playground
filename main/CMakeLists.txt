# PlatformIO/ESP-IDF only evaluates this list during CMake configuration.
# Adding a new source file therefore requires a reconfigure (editing this file
# is sufficient); ordinary edits keep using the incremental object cache.
# Keep the source glob explicit in this component so newly added PGOS apps,
# including standalone game engines, trigger a CMake reconfigure in PlatformIO.
file(GLOB_RECURSE PGOS_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

idf_component_register(
    SRCS "main.c" ${PGOS_SOURCES}
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src"
    REQUIRES bluepad32 bluepad32_arduino arduino btstack lvgl
             esp_lcd esp_driver_spi esp_lcd_ili9341)

target_compile_definitions(${COMPONENT_LIB} PRIVATE PGOS_BLE_GAMEPAD_BACKEND=1)
target_include_directories(${COMPONENT_LIB} PRIVATE
                           "${CMAKE_SOURCE_DIR}/src")
