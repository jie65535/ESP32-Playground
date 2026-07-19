// Bluepad32 / BTstack application entry point.  Arduino setup()/loop() are
// started by bluepad32_arduino on the Arduino task, while BTstack owns the
// Bluetooth task and run loop.

#include "sdkconfig.h"

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <btstack_stdio_esp32.h>

#include <esp_log.h>

#include <arduino_platform.h>
#include <uni.h>

int app_main(void) {
    // Arduino 3.x's ESP.getSketchSize() validates the running image.  The
    // ESP-IDF image verifier logs every segment at INFO, which would flood
    // the USB console because PGOS samples runtime resources twice per
    // second. Keep the validation but make it silent during normal runtime.
    esp_log_level_set("esp_image", ESP_LOG_ERROR);

#ifndef CONFIG_ESP_CONSOLE_UART_NONE
#ifndef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
    btstack_stdio_init();
#endif
#endif

    btstack_init();
    uni_platform_set_custom(get_arduino_platform());
    uni_init(0, NULL);
    btstack_run_loop_execute();
    return 0;
}
