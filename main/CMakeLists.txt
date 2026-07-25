# ESP-IDF evaluates this component file once in script mode while collecting
# requirements, so CONFIGURE_DEPENDS cannot be used here. Keep this comment in
# the same change as newly added standalone game/UI sources to force
# PlatformIO's CMake reconfiguration and refresh the globbed source list,
# including the generated Platformer campaign pack, sparse level runtime and
# tile renderer, the complete Minesweeper app/engine/solver/records/layout and
# the Microphone settings app, SpeexDSP denoiser, WebRTC VAD and voice
# enhancer, plus the TinyLM app, inference service and corpus-derived font.
file(GLOB_RECURSE PGOS_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE LIBFVAD_SOURCES
     "${CMAKE_SOURCE_DIR}/src/third_party/libfvad/src/*.c")
file(GLOB SPEEXDSP_SOURCES
     "${CMAKE_SOURCE_DIR}/src/third_party/speexdsp/src/*.c")

idf_component_register(
    SRCS "main.c" ${PGOS_SOURCES} ${LIBFVAD_SOURCES} ${SPEEXDSP_SOURCES}
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src"
                 "${CMAKE_SOURCE_DIR}/src/third_party/speexdsp/include"
                 "${CMAKE_SOURCE_DIR}/src/third_party/speexdsp/src"
    REQUIRES bluepad32 bluepad32_arduino arduino btstack lvgl
             esp_lcd esp_driver_spi esp_lcd_ili9341)

target_compile_definitions(${COMPONENT_LIB} PRIVATE PGOS_BLE_GAMEPAD_BACKEND=1)
set_source_files_properties(${SPEEXDSP_SOURCES} PROPERTIES
                            COMPILE_DEFINITIONS HAVE_CONFIG_H=1)
set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/src/services/TinyLmService.cpp"
    PROPERTIES COMPILE_OPTIONS "-O3")
target_include_directories(${COMPONENT_LIB} PRIVATE
                           "${CMAKE_SOURCE_DIR}/src")
