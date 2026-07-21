# ESP-IDF evaluates this component file once in script mode while collecting
# requirements, so CONFIGURE_DEPENDS cannot be used here. Keep this comment in
# the same change as newly added standalone game/UI sources to force
# PlatformIO's CMake reconfiguration and refresh the globbed source list,
# including the generated Platformer campaign pack, sparse level runtime and
# tile renderer.
file(GLOB_RECURSE PGOS_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

idf_component_register(
    SRCS "main.c" ${PGOS_SOURCES}
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src"
    REQUIRES bluepad32 bluepad32_arduino arduino btstack lvgl
             esp_lcd esp_driver_spi esp_lcd_ili9341)

target_compile_definitions(${COMPONENT_LIB} PRIVATE PGOS_BLE_GAMEPAD_BACKEND=1)
target_include_directories(${COMPONENT_LIB} PRIVATE
                           "${CMAKE_SOURCE_DIR}/src")
