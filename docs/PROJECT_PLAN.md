# Playground 项目路线

长期平台方向：PlaygroundOS（PGOS）。先建立稳定的服务、应用和连接边界，再逐项加入板载外设与实验应用。

## 已完成：屏幕与板卡信息

目标是建立可重复的最小固件：

- 点亮 ILI9341 横屏。
- 显示芯片、Flash、PSRAM、运行时间和空闲内存。
- 通过 USB CDC 输出启动信息和低频状态。
- 不连接外部模块，不依赖外部按键，不启用 Wi-Fi。
- 使用 PSRAM 离屏 Sprite，支持 USB 键盘控制和 RGB565 无损截图。

## 当前：Wi-Fi Station

PlaygroundOS 第一阶段基座已建立：`SystemKernel`、`AppManager`、`DisplayService`、`ConsoleService` 和 `WifiService` 已在 `src/` 下分层，现有三个页面作为前台 App 运行。

- 连接家庭 2.4 GHz 路由器。
- 显示 SSID、IP、RSSI 和重连次数。
- USB 命令支持 `wifi status`、`wifi scan`、`wifi reconnect`。
- 使用非阻塞连接状态机和退避重连。
- 通过 USB 控制台扫描、选择 SSID 和输入密码；密码只写入设备 NVS，不进入 Git。
- 支持隐藏 SSID 的手工输入和开放网络。
- NTP 状态显示留待入网稳定后单独增加。

## 随后：局域网 HTTP/JSON

- ESP32 提供 `/api/v1/status` 和 `/api/v1/telemetry`。
- ESP32 周期性向电脑服务器发送 heartbeat。
- Python 服务器显示设备在线、IP、RSSI、内存和最近事件。
- 命令接口使用白名单和局域网 token。

## 其它实验

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
