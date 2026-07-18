# ESP32 Playground

面向 QD 电子 ES3N28P / ESP32-S3 R8N16 开发板的个人实验项目。

未来的可扩展设备基座命名为 PlaygroundOS（PGOS）：用统一的应用、服务、事件和连接抽象承载显示、Wi‑Fi、BLE、音频、网络控制台和小游戏实验。

这是一个面向 ESP32-S3 开发板的个人实验空间。目标是用小步实验把显示、USB、GPIO、RGB、音频、I²C、Wi-Fi、局域网通信和其它外设逐个玩明白，并把真实接线和实测结果记录下来。

当前固件点亮 ILI9341 屏幕，显示芯片、内存、运行时间和板卡信息，并开始 Wi-Fi Station 实验：通过 USB 控制台扫描/选择网络、输入密码并保存到设备 NVS，连接状态、IP 和 RSSI 同时显示在屏幕与控制台。

当前固件使用 LVGL 9.5 retained-mode UI：系统桌面、系统信息、显示实验和网络设置由统一主题、状态栏、页面转场和焦点卡片组成。LVGL 使用 40 行 RGB565 局部缓冲，显示服务在 PSRAM 中维护完整 shadow framebuffer，因此仍保留无损截图协议；USB 状态日志只低频输出。

## 当前环境

- PlatformIO + Arduino
- `espressif32 @ 7.0.1`
- `es3n28p_r8n16`
- `lvgl @ 9.5.0`
- ESP32-S3，16MB QIO Flash，8MB OPI PSRAM
- ILI9341V，逻辑分辨率 320×240 横屏
- 原生 USB CDC，`Serial` 波特率 115200

## 第一个固件

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COMx
pio device monitor --port COMx --baud 115200
```

当前固件不连接外部业务模块；Wi-Fi 凭据只保存在设备 NVS，不进入 Git。USB 输出启动信息、Wi-Fi 状态和低频运行日志。

## USB 控制台和截图

安装依赖后可使用箭头键、回车、快捷键和无损截图：

```powershell
python -m pip install -r tools/requirements.txt
python tools/playground_console.py --list
python tools/playground_console.py --port COM3
python tools/playground_console.py --port COM3 --wifi-setup
python tools/capture_screen.py --port COM3 --output captures/home.png
```

控制台的 `1/2/3` 可直接跳页，`B` 返回桌面，`C` 显示色卡，`R` 查询状态，`W` 启动 Wi-Fi 配网，`S` 会暂停日志读取线程并导出 LVGL shadow framebuffer 的原始 RGB565 画布到 PNG/Windows 剪贴板。方向键和回车只是可选便利，`Q` 退出控制台。固件也接受 `home`、`wifi scan`、`wifi select <index>`、`wifi password <value>`、`wifi status`、`wifi reconnect` 和 `wifi clear` 等换行命令；密码不会由固件或控制台回显。推荐用 `--wifi-setup` 的隐藏输入流程，不要把密码直接写在 shell 命令行中，以免进入主机历史记录。

局域网 TCP 控制台服务器：接收 HELLO/heartbeat，发送 PING，并可在交互提示符中发送白名单命令：

```powershell
python tools/pgos_server.py --listen 0.0.0.0 --port 19000
```

设备端配置服务器地址并启用主动连接：

```text
server set 192.168.1.4 19000
server on
server status
```

服务器连接后可直接输入 `up`、`down`、`ok`、`status`、`page system`、`page display` 或 `page network`。设备返回 ACK；`status` 还会返回结构化 STATE。Wi-Fi 密码、清除凭据等敏感命令不会通过 TCP 执行。

## 文档入口

- [项目交接与工作规则](AGENTS.md)
- [硬件和引脚基线](docs/HARDWARE.md)
- [项目路线](docs/PROJECT_PLAN.md)
- [微型应用平台架构](docs/ARCHITECTURE.md)
- [Wi-Fi 与局域网方案](docs/WIFI_PLAN.md)
- [实验记录：屏幕和板卡信息](docs/experiments/001_display_info.md)
- [实验记录：Wi-Fi Station](docs/experiments/002_wifi_station.md)
- [实验记录：TCP 诊断服务器](docs/experiments/003_tcp_server.md)
- [实验记录：TCP 应用层吞吐](docs/experiments/004_tcp_throughput.md)
- [实验记录：PGOS UI v2 / LVGL 9.5](docs/experiments/005_ui_v2_lvgl.md)
- [可复用经验知识库](docs/knowledge/README.md)
- [厂商原始资料说明](docs/vendor/README.md)

原始厂商资料包位于 `docs/vendor/2.8inch_IPS_ESP32-S3_ILI9341V_ES3C28P_ES3N28P_V1.0`。该目录是参考资料，不是程序依赖；缺失时不应复制或修改其内容。
