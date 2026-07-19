# BLE gamepad probe

这是实验 013 的最小 Xbox BLE 手柄探针源码，不直接加入 PGOS 生产环境。

它需要以官方 [ESP-IDF + Arduino + Bluepad32 template](https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template) 为基座：

1. 克隆模板及 submodule。
2. 用本目录的 `sketch.cpp` 替换模板 `main/sketch.cpp`。
3. 把 `sdkconfig.defaults.fragment` 追加到模板 `sdkconfig.defaults`；若已有同名配置，以本文件为准。
4. 删除模板生成的 `sdkconfig.<env>`，再重新构建。

本次验证使用：

- pioarduino platform `54.03.21`
- ESP-IDF `5.4.2`
- Bluepad32 `4.2.0`
- BTstack `1.6.2`
- PlatformIO target `esp32-s3-devkitc-1`（仅作协议探针；生产集成必须改回 ES3N28P 的 16MB Flash / 8MB OPI PSRAM 板型）

探针启动后持续扫描 BLE 手柄。按住 Xbox 手柄 Pair 键约 3 秒；连接后串口输出 VID/PID、D-pad、按键、四轴、扳机和电量。X 键上升沿触发 1.5 秒、双马达 100% 震动，用于明确验证输出报告。

USB 日志使用 ESP32-S3 原生 USB Serial/JTAG，不使用模板内置的 UART REPL。不要同时启用 `CONFIG_ESP_CONSOLE_UART_NONE`；它会覆盖 USB 主控制台选择。
