# Playground 项目路线

## v0.1：屏幕与板卡信息

目标是建立可重复的最小固件：

- 点亮 ILI9341 横屏。
- 显示芯片、Flash、PSRAM、运行时间和空闲内存。
- 通过 USB CDC 输出每秒状态。
- 不连接 BMS，不依赖外部按键，不启用 Wi-Fi。

## v0.2：Wi-Fi Station

- 连接家庭 2.4 GHz 路由器。
- 显示 SSID、IP、RSSI、重连次数和 NTP 状态。
- USB 命令支持 `wifi status`、`wifi scan`、`wifi reconnect`。
- 使用非阻塞连接状态机和退避重连。
- SSID/密码先放在未跟踪本地配置，后续再做 NVS 配网。

## v0.3：局域网 HTTP/JSON

- ESP32 提供 `/api/v1/status` 和 `/api/v1/telemetry`。
- ESP32 周期性向电脑服务器发送 heartbeat。
- Python 服务器显示设备在线、IP、RSSI、内存和最近事件。
- 命令接口使用白名单和局域网 token。

## v0.4 及以后

按一次只探索一个外设或协议：

- RGB 和按键
- ES8311 播放
- 麦克风采集
- I²C 扫描和 PCF8563
- ADC 与功耗
- BLE
- TF/扩展接口（以原理图和实物为准）
- WebSocket、MQTT、OTA

MQTT 和 OTA 不作为 Wi-Fi 第一版的前置条件。
