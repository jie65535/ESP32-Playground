// Temporary ESP32-S3 / Xbox BLE gamepad probe for ESP32 Playground.

#include <Arduino.h>
#include <Bluepad32.h>

#include <cstdio>

ControllerPtr controllers[BP32_MAX_GAMEPADS];
uint16_t previousButtons[BP32_MAX_GAMEPADS] = {};

void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
        if (controllers[i] != nullptr) {
            continue;
        }
        controllers[i] = ctl;
        const ControllerProperties properties = ctl->getProperties();
        std::printf("[gamepad] connected slot=%d model=%s vid=%04x pid=%04x\n",
                    i, ctl->getModelName().c_str(), properties.vendor_id,
                    properties.product_id);
        std::fflush(stdout);
        return;
    }
    std::printf("[gamepad] connected but no free slot\n");
    std::fflush(stdout);
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
        if (controllers[i] != ctl) {
            continue;
        }
        controllers[i] = nullptr;
        previousButtons[i] = 0;
        std::printf("[gamepad] disconnected slot=%d\n", i);
        std::fflush(stdout);
        return;
    }
}

void setup() {
    std::printf("[probe] Bluepad32 %s\n", BP32.firmwareVersion());
    const uint8_t* address = BP32.localBdAddress();
    std::printf("[probe] local=%02x:%02x:%02x:%02x:%02x:%02x\n",
                address[0], address[1], address[2], address[3], address[4],
                address[5]);
    std::printf("[probe] scanning for BLE gamepads; hold Xbox Pair for 3 seconds\n");
    std::fflush(stdout);

    BP32.setup(&onConnectedController, &onDisconnectedController, true);
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);
}

void loop() {
    if (BP32.update()) {
        for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
            ControllerPtr ctl = controllers[i];
            if (ctl == nullptr || !ctl->isConnected() || !ctl->hasData() ||
                !ctl->isGamepad()) {
                continue;
            }

            const uint16_t buttons = ctl->buttons();
            const bool xPressed = (buttons & BUTTON_X) != 0 &&
                                  (previousButtons[i] & BUTTON_X) == 0;
            previousButtons[i] = buttons;
            if (xPressed) {
                std::printf("[rumble] requested duration=1500 weak=255 strong=255\n");
                std::fflush(stdout);
                ctl->playDualRumble(0, 1500, 0xff, 0xff);
            }

            std::printf(
                "[input] slot=%d dpad=%02x buttons=%04x misc=%04x "
                "lx=%d ly=%d rx=%d ry=%d lt=%d rt=%d battery=%u\n",
                i, ctl->dpad(), buttons, ctl->miscButtons(), ctl->axisX(),
                ctl->axisY(), ctl->axisRX(), ctl->axisRY(), ctl->brake(),
                ctl->throttle(), ctl->battery());
            std::fflush(stdout);
        }
    }
    delay(10);
}
